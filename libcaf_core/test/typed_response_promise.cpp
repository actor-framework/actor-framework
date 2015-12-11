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

#include <map>

#include "caf/config.hpp"

#define CAF_SUITE typed_response_promise
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

using foo_actor = typed_actor<replies_to<get_atom, int>::with<int>,
                              replies_to<get_atom, int, int>::with<int, int>,
                              reacts_to<put_atom, int, int>,
                              reacts_to<put_atom, int, int, int>>;

using foo_promise = typed_response_promise<int>;
using foo2_promise = typed_response_promise<int, int>;

class foo_actor_impl : public foo_actor::base {
public:
  foo_actor_impl(actor_config& cfg) : foo_actor::base(cfg) {
    // nop
  }

  behavior_type make_behavior() override {
    return {
      [=](get_atom, int x) -> foo_promise {
        auto calculator = spawn([](event_based_actor* self) -> behavior {
          return {
            [self](int promise_id, int value) -> message {
              self->quit();
              return make_message(put_atom::value, promise_id, value * 2);
            }
          };
        });
        send(calculator, next_id_, x);
        auto& entry = promises_[next_id_++];
        entry = make_response_promise();
        return entry;
      },
      [=](get_atom, int x, int y) -> foo2_promise {
        auto calculator = spawn([](event_based_actor* self) -> behavior {
          return {
            [self](int promise_id, int v0, int v1) -> message {
              self->quit();
              return make_message(put_atom::value, promise_id, v0 * 2, v1 * 2);
            }
          };
        });
        send(calculator, next_id_, x, y);
        auto& entry = promises2_[next_id_++];
        entry = make_response_promise();
        return entry;
      },
      [=](put_atom, int promise_id, int x) {
        auto i = promises_.find(promise_id);
        if (i == promises_.end())
          return;
        i->second.deliver(x);
        promises_.erase(i);
      },
      [=](put_atom, int promise_id, int x, int y) {
        auto i = promises2_.find(promise_id);
        if (i == promises2_.end())
          return;
        i->second.deliver(x, y);
        promises2_.erase(i);
      }
    };
  }

private:
  int next_id_ = 0;
  std::map<int, foo_promise> promises_;
  std::map<int, foo2_promise> promises2_;
};

struct fixture {
  actor_system system;

  ~fixture() {
    system.await_all_actors_done();
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(typed_spawn_tests, fixture)

CAF_TEST(typed_response_promise) {
  scoped_actor self{system};
  auto foo = self->spawn<foo_actor_impl>();
  self->request(foo, get_atom::value, 42).await(
    [](int x) {
      CAF_CHECK_EQUAL(x, 84);
    }
  );
  self->request(foo, get_atom::value, 42, 52).await(
    [](int x, int y) {
      CAF_CHECK_EQUAL(x, 84);
      CAF_CHECK_EQUAL(y, 104);
    }
  );
  self->send_exit(foo, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
