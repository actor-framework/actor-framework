// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_pool.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

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
    actor_system system;
  };
  union {
    scoped_execution_unit context;
  };

  std::function<actor()> spawn_worker;

  fixture() {
    new (&system) actor_system(cfg);
    new (&context) scoped_execution_unit(&system);
    spawn_worker = [&] { return system.spawn<worker>(); };
  }

  ~fixture() {
    system.await_all_actors_done();
    context.~scoped_execution_unit();
    system.~actor_system();
    if (s_dtors.load() != s_ctors.load()) {
      CAF_RAISE_ERROR("ctor / dtor mismatch");
    }
  }
};

WITH_FIXTURE(fixture) {

TEST("round_robin_actor_pool") {
  scoped_actor self{system};
  auto pool = actor_pool::make(&context, 5, spawn_worker,
                               actor_pool::round_robin());
  self->send(pool, sys_atom_v, put_atom_v, spawn_worker());
  std::vector<actor> workers;
  for (int32_t i = 0; i < 6; ++i) {
    self->request(pool, infinite, i, i)
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
  self->request(pool, infinite, sys_atom_v, get_atom_v)
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
    self->request(pool, infinite, sys_atom_v, get_atom_v)
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
  scoped_actor self{system};
  auto spawn5 = [&] {
    return actor_pool::make(&context, 5, fixture::spawn_worker,
                            actor_pool::broadcast());
  };
  check_eq(system.registry().running(), 1u);
  auto pool = actor_pool::make(&context, 5, spawn5, actor_pool::broadcast());
  check_eq(system.registry().running(), 32u);
  self->send(pool, 1, 2);
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
  scoped_actor self{system};
  auto pool = actor_pool::make(&context, 5, spawn_worker, actor_pool::random());
  for (int i = 0; i < 5; ++i) {
    self->request(pool, std::chrono::milliseconds(250), 1, 2)
      .receive([&](int res) { check_eq(res, 3); }, HANDLE_ERROR);
  }
  self->send_exit(pool, exit_reason::user_shutdown);
}

} // WITH_FIXTURE(fixture)

} // namespace
