/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE actor_pool
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

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
    set_exit_handler([=](scheduled_actor* self, exit_msg& em) {
      nested(self, em);
    });
    return {
      [](int x, int y) {
        return x + y;
      }
    };
  }
};

struct fixture {
  // allows us to check s_dtors after dtor of actor_system
  actor_system_config cfg;
  union { actor_system system; };
  union { scoped_execution_unit context; };

  std::function<actor ()> spawn_worker;

  fixture() {
    new (&system) actor_system(cfg);
    new (&context) scoped_execution_unit(&system);
    spawn_worker = [&] {
      return system.spawn<worker>();
    };
  }

  ~fixture() {
    system.await_all_actors_done();
    context.~scoped_execution_unit();
    system.~actor_system();
    CAF_CHECK_EQUAL(s_dtors.load(), s_ctors.load());
  }
};

void handle_err(const error& err) {
  CAF_FAIL("AUT responded with an error: " + to_string(err));
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(actor_pool_tests, fixture)

CAF_TEST(round_robin_actor_pool) {
  scoped_actor self{system};
  auto pool = actor_pool::make(&context, 5, spawn_worker,
                               actor_pool::round_robin());
  self->send(pool, sys_atom::value, put_atom::value, spawn_worker());
  std::vector<actor> workers;
  for (int i = 0; i < 6; ++i) {
    self->request(pool, infinite, i, i).receive(
      [&](int res) {
        CAF_CHECK_EQUAL(res, i + i);
        auto sender = actor_cast<strong_actor_ptr>(self->current_sender());
        CAF_REQUIRE(sender);
        workers.push_back(actor_cast<actor>(std::move(sender)));
      },
      handle_err
    );
  }
  CAF_CHECK_EQUAL(workers.size(), 6u);
  CAF_CHECK(std::unique(workers.begin(), workers.end()) == workers.end());
  self->request(pool, infinite, sys_atom::value, get_atom::value).receive(
    [&](std::vector<actor>& ws) {
      std::sort(workers.begin(), workers.end());
      std::sort(ws.begin(), ws.end());
      CAF_REQUIRE_EQUAL(workers.size(), ws.size());
      CAF_CHECK(std::equal(workers.begin(), workers.end(), ws.begin()));
    },
    handle_err
  );
  CAF_MESSAGE("await last worker");
  anon_send_exit(workers.back(), exit_reason::user_shutdown);
  self->wait_for(workers.back());
  CAF_MESSAGE("last worker shut down");
  workers.pop_back();
  // poll actor pool up to 10 times or until it removes the failed worker
  bool success = false;
  size_t i = 0;
  while (!success && ++i <= 10) {
    self->request(pool, infinite, sys_atom::value, get_atom::value).receive(
      [&](std::vector<actor>& ws) {
        success = workers.size() == ws.size();
        if (success) {
          std::sort(ws.begin(), ws.end());
          CAF_CHECK_EQUAL(workers, ws);
        } else {
          // wait a bit until polling again
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
      },
      handle_err
    );
  }
  CAF_REQUIRE_EQUAL(success, true);
  CAF_MESSAGE("about to send exit to workers");
  self->send_exit(pool, exit_reason::user_shutdown);
  self->wait_for(workers);
}

CAF_TEST(broadcast_actor_pool) {
  scoped_actor self{system};
  auto spawn5 = [&] {
    return actor_pool::make(&context, 5, fixture::spawn_worker,
                            actor_pool::broadcast());
  };
  CAF_CHECK_EQUAL(system.registry().running(), 1u);
  auto pool = actor_pool::make(&context, 5, spawn5, actor_pool::broadcast());
  CAF_CHECK_EQUAL(system.registry().running(), 32u);
  self->send(pool, 1, 2);
  std::vector<int> results;
  int i = 0;
  self->receive_for(i, 25)(
    [&](int res) {
      results.push_back(res);
    },
    after(std::chrono::milliseconds(250)) >> [] {
      CAF_ERROR("didn't receive a result");
    }
  );
  CAF_CHECK_EQUAL(results.size(), 25u);
  CAF_CHECK(std::all_of(results.begin(), results.end(),
                        [](int res) { return res == 3; }));
  self->send_exit(pool, exit_reason::user_shutdown);
}

CAF_TEST(random_actor_pool) {
  scoped_actor self{system};
  auto pool = actor_pool::make(&context, 5, spawn_worker, actor_pool::random());
  for (int i = 0; i < 5; ++i) {
    self->request(pool, std::chrono::milliseconds(250), 1, 2).receive(
      [&](int res) {
        CAF_CHECK_EQUAL(res, 3);
      },
      handle_err
    );
  }
  self->send_exit(pool, exit_reason::user_shutdown);
}

CAF_TEST(split_join_actor_pool) {
  auto spawn_split_worker = [&] {
    return system.spawn<lazy_init>([]() -> behavior {
      return {
        [](size_t pos, const std::vector<int>& xs) {
          return xs[pos];
        }
      };
    });
  };
  auto split_fun = [](std::vector<std::pair<actor, message>>& xs, message& y) {
    for (size_t i = 0; i < xs.size(); ++i) {
      xs[i].second = make_message(i) + y;
    }
  };
  auto join_fun = [](int& res, message& msg) {
    msg.apply([&](int x) {
      res += x;
    });
  };
  scoped_actor self{system};
  CAF_MESSAGE("create actor pool");
  auto pool = actor_pool::make(&context, 5, spawn_split_worker,
                            actor_pool::split_join<int>(join_fun, split_fun));
  self->request(pool, infinite, std::vector<int>{1, 2, 3, 4, 5}).receive(
    [&](int res) {
      CAF_CHECK_EQUAL(res, 15);
    },
    handle_err
  );
  self->request(pool, infinite, std::vector<int>{6, 7, 8, 9, 10}).receive(
    [&](int res) {
      CAF_CHECK_EQUAL(res, 40);
    },
    handle_err
  );
  self->send_exit(pool, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
