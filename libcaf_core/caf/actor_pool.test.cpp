// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_pool.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_registry.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;
using namespace std::literals;

#define HANDLE_ERROR                                                           \
  [](const error& err) {                                                       \
    test::runnable::current().fail("AUT responded with an error: {}",          \
                                   to_string(err));                            \
  }

namespace {

std::atomic<size_t> s_ctors;
std::atomic<size_t> s_dtors;

class worker : public event_based_actor {
public:
  worker(actor_config& cfg) : event_based_actor(cfg) {
    ++s_ctors;
  }

  ~worker() override {
    ++s_dtors;
  }

  behavior make_behavior() override {
    auto nested = exit_handler_;
    set_exit_handler(
      [=](scheduled_actor* self, exit_msg& em) { nested(self, em); });
    return {
      [](int32_t x, int32_t y) { return x + y; },
    };
  }
};

struct fixture {
  // allows us to check s_dtors after dtor of actor_system
  actor_system_config cfg;
  union {
    actor_system sys;
  };

  std::function<actor()> spawn_worker;

  fixture() {
    new (&sys) actor_system(cfg);
    spawn_worker = [&] { return sys.spawn<worker>(); };
  }

  ~fixture() {
    sys.await_all_actors_done();
    sys.~actor_system();
    if (s_dtors.load() != s_ctors.load()) {
      CAF_RAISE_ERROR("ctor / dtor mismatch");
    }
  }
};

WITH_FIXTURE(fixture) {

TEST("round_robin_actor_pool") {
  scoped_actor self{sys};
  auto pool = actor_pool::make(sys, 5, spawn_worker, actor_pool::round_robin());
  self->mail(sys_atom_v, put_atom_v, spawn_worker()).send(pool);
  std::vector<actor> workers;
  for (int32_t i = 0; i < 6; ++i) {
    self->mail(i, i)
      .request(pool, infinite)
      .receive(
        [&](int32_t res) {
          check_eq(res, i + i);
          auto sender = actor_cast<strong_actor_ptr>(self->current_sender());
          require(static_cast<bool>(sender));
          workers.push_back(actor_cast<actor>(std::move(sender)));
        },
        HANDLE_ERROR);
  }
  check_eq(workers.size(), 6u);
  check(std::unique(workers.begin(), workers.end()) == workers.end());
  self->mail(sys_atom_v, get_atom_v)
    .request(pool, infinite)
    .receive(
      [&](std::vector<actor>& ws) {
        std::sort(workers.begin(), workers.end());
        std::sort(ws.begin(), ws.end());
        require_eq(workers.size(), ws.size());
        check(std::equal(workers.begin(), workers.end(), ws.begin()));
      },
      HANDLE_ERROR);
  log::test::debug("await last worker");
  anon_send_exit(workers.back(), exit_reason::user_shutdown);
  self->wait_for(workers.back());
  log::test::debug("last worker shut down");
  workers.pop_back();
  // poll actor pool up to 10 times or until it removes the failed worker
  bool success = false;
  size_t i = 0;
  while (!success && ++i <= 10) {
    self->mail(sys_atom_v, get_atom_v)
      .request(pool, infinite)
      .receive(
        [&](std::vector<actor>& ws) {
          success = workers.size() == ws.size();
          if (success) {
            std::sort(ws.begin(), ws.end());
            check_eq(workers, ws);
          } else {
            // wait a bit until polling again
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
          }
        },
        HANDLE_ERROR);
  }
  require_eq(success, true);
  log::test::debug("about to send exit to workers");
  self->send_exit(pool, exit_reason::user_shutdown);
  self->wait_for(workers);
}

TEST("broadcast_actor_pool") {
  scoped_actor self{sys};
  auto spawn5 = [&] {
    return actor_pool::make(sys, 5, fixture::spawn_worker,
                            actor_pool::broadcast());
  };
  check_eq(sys.registry().running(), 1u);
  auto pool = actor_pool::make(sys, 5, spawn5, actor_pool::broadcast());
  check_eq(sys.registry().running(), 32u);
  self->mail(1, 2).send(pool);
  std::vector<int> results;
  int i = 0;
  self->receive_for(i, 25)([&](int res) { results.push_back(res); },
                           after(std::chrono::milliseconds(250)) >>
                             [this] { fail("didn't receive a result"); });
  check_eq(results.size(), 25u);
  check(std::all_of(results.begin(), results.end(),
                    [](int res) { return res == 3; }));
  self->send_exit(pool, exit_reason::user_shutdown);
}

TEST("random_actor_pool") {
  scoped_actor self{sys};
  auto pool = actor_pool::make(sys, 5, spawn_worker, actor_pool::random());
  for (int i = 0; i < 5; ++i) {
    self->mail(1, 2)
      .request(pool, 250ms)
      .receive([&](int res) { check_eq(res, 3); }, HANDLE_ERROR);
  }
  self->send_exit(pool, exit_reason::user_shutdown);
}

} // WITH_FIXTURE(fixture)

} // namespace
