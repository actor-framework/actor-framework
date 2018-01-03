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

#define CAF_SUITE constructor_attach
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

using die_atom = atom_constant<atom("die")>;
using done_atom = atom_constant<atom("done")>;

class testee : public event_based_actor {
public:
  testee(actor_config& cfg, actor buddy) : event_based_actor(cfg) {
    attach_functor([=](const error& reason) {
      send(buddy, done_atom::value, reason);
    });
  }

  behavior make_behavior() override {
    return {
      [=](die_atom) {
        quit(exit_reason::user_shutdown);
      }
    };
  }
};

class spawner : public event_based_actor {
public:
  spawner(actor_config& cfg)
      : event_based_actor(cfg),
        downs_(0),
        testee_(spawn<testee, monitored>(this)) {
    set_down_handler([=](down_msg& msg) {
      CAF_CHECK_EQUAL(msg.reason, exit_reason::user_shutdown);
      CAF_CHECK_EQUAL(msg.source, testee_.address());
      if (++downs_ == 2)
        quit(msg.reason);
    });
  }

  behavior make_behavior() override {
    return {
      [=](done_atom, const error& reason) {
        CAF_CHECK_EQUAL(reason, exit_reason::user_shutdown);
        if (++downs_ == 2) {
          quit(reason);
        }
      },
      [=](die_atom x) {
        return delegate(testee_, x);
      }
    };
  }

  void on_exit() override {
    destroy(testee_);
  }

private:
  int downs_;
  actor testee_;
};

} // namespace <anonymous>

CAF_TEST(constructor_attach) {
  actor_system_config cfg;
  actor_system system{cfg};
  anon_send(system.spawn<spawner>(), die_atom::value);
}
