/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE actor_pool
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

std::atomic<size_t> s_ctors;
std::atomic<size_t> s_dtors;

class worker : public event_based_actor {
public:
  worker();
  ~worker();
  behavior make_behavior() override;
};

worker::worker() {
  ++s_ctors;
}

worker::~worker() {
  ++s_dtors;
}

behavior worker::make_behavior() {
  return {
    [](int x, int y) {
      return x + y;
    }
  };
}

actor spawn_worker() {
  return spawn<worker>();
}

struct fixture {
  fixture() {
    announce<std::vector<int>>("vector<int>");
  }
  ~fixture() {
    await_all_actors_done();
    shutdown();
    CAF_CHECK_EQUAL(s_dtors.load(), s_ctors.load());
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(actor_pool_tests, fixture)

CAF_TEST(round_robin_actor_pool) {
  scoped_actor self;
  auto w = actor_pool::make(5, spawn_worker, actor_pool::round_robin{});
  self->monitor(w);
  self->send(w, sys_atom::value, put_atom::value, spawn_worker());
  std::vector<actor_addr> workers;
  for (int i = 0; i < 6; ++i) {
    self->sync_send(w, i, i).await(
      [&](int res) {
        CAF_CHECK_EQUAL(res, i + i);
        auto sender = self->current_sender();
        self->monitor(sender);
        workers.push_back(sender);
      }
    );
  }
  CAF_CHECK(workers.size() == 6);
  CAF_CHECK(std::unique(workers.begin(), workers.end()) == workers.end());
  auto is_invalid = [](const actor_addr& addr) {
    return addr == invalid_actor_addr;
  };
  CAF_CHECK(std::none_of(workers.begin(), workers.end(), is_invalid));
  self->sync_send(w, sys_atom::value, get_atom::value).await(
    [&](std::vector<actor>& ws) {
      std::sort(workers.begin(), workers.end());
      std::sort(ws.begin(), ws.end());
      CAF_CHECK(workers.size() == ws.size()
                && std::equal(workers.begin(), workers.end(), ws.begin()));
    }
  );
  anon_send_exit(workers.back(), exit_reason::user_shutdown);
  self->receive(
    after(std::chrono::milliseconds(25)) >> [] {
      // wait some time to give the pool time to remove the failed worker
    }
  );
  self->receive(
    [&](const down_msg& dm) {
      CAF_CHECK(dm.source == workers.back());
      workers.pop_back();
      // check whether actor pool removed failed worker
      self->sync_send(w, sys_atom::value, get_atom::value).await(
        [&](std::vector<actor>& ws) {
          std::sort(ws.begin(), ws.end());
          CAF_CHECK(workers.size() == ws.size()
                    && std::equal(workers.begin(), workers.end(), ws.begin()));
        }
      );
    },
    after(std::chrono::milliseconds(250)) >> [] {
      CAF_TEST_ERROR("didn't receive a down message");
    }
  );
  CAF_MESSAGE("about to send exit to workers");
  self->send_exit(w, exit_reason::user_shutdown);
  for (int i = 0; i < 6; ++i) {
    self->receive(
      [&](const down_msg& dm) {
        auto last = workers.end();
        auto src = dm.source;
        CAF_CHECK(src != invalid_actor_addr);
        auto pos = std::find(workers.begin(), last, src);
        //CAF_CHECK(pos != last || src == w); fail?
        if (pos != last) {
          workers.erase(pos);
        }
      },
      after(std::chrono::milliseconds(250)) >> [] {
        CAF_TEST_ERROR("didn't receive a down message");
      }
    );
  }
}

CAF_TEST(broadcast_actor_pool) {
  scoped_actor self;
  auto spawn5 = []() {
    return actor_pool::make(5, spawn_worker, actor_pool::broadcast{});
  };
  auto w = actor_pool::make(5, spawn5, actor_pool::broadcast{});
  self->send(w, 1, 2);
  std::vector<int> results;
  int i = 0;
  self->receive_for(i, 25)(
    [&](int res) {
      results.push_back(res);
    },
    after(std::chrono::milliseconds(250)) >> [] {
      CAF_TEST_ERROR("didn't receive a result");
    }
  );
  CAF_CHECK_EQUAL(results.size(), 25);
  CAF_CHECK(std::all_of(results.begin(), results.end(),
                        [](int res) { return res == 3; }));
  self->send_exit(w, exit_reason::user_shutdown);
}

CAF_TEST(random_actor_pool) {
  scoped_actor self;
  auto w = actor_pool::make(5, spawn_worker, actor_pool::random{});
  for (int i = 0; i < 5; ++i) {
    self->sync_send(w, 1, 2).await(
      [&](int res) {
        CAF_CHECK_EQUAL(res, 3);
      },
      after(std::chrono::milliseconds(250)) >> [] {
        CAF_TEST_ERROR("didn't receive a down message");
      }
    );
  }
  self->send_exit(w, exit_reason::user_shutdown);
}

CAF_TEST(split_join_actor_pool) {
  auto spawn_split_worker = [] {
    return spawn<lazy_init>([]() -> behavior {
      return {
        [](size_t pos, std::vector<int> xs) {
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
  scoped_actor self;
  auto w = actor_pool::make(5, spawn_split_worker,
                            actor_pool::split_join<int>(join_fun, split_fun));
  self->sync_send(w, std::vector<int>{1, 2, 3, 4, 5}).await(
    [&](int res) {
      CAF_CHECK_EQUAL(res, 15);
    }
  );
  self->sync_send(w, std::vector<int>{6, 7, 8, 9, 10}).await(
    [&](int res) {
      CAF_CHECK_EQUAL(res, 40);
    }
  );
  self->send_exit(w, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
