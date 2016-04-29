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

#include "caf/config.hpp"

#define CAF_SUITE constructor_attach
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

using die_atom = atom_constant<atom("die")>;
using done_atom = atom_constant<atom("done")>;

} // namespace <anonymous>

CAF_TEST(constructor_attach) {
  class testee : public event_based_actor {
  public:
    testee(actor_config& cfg, actor buddy) : event_based_actor(cfg) {
      attach_functor([=](const error& reason) {
        send(buddy, done_atom::value, reason);
      });
    }

    behavior make_behavior() {
      return {
        [=](die_atom) {
          quit(exit_reason::user_shutdown);
        }
      };
    }
  };
  class spawner : public event_based_actor {
  public:
    spawner(actor_config& cfg) : event_based_actor(cfg), downs_(0) {
      // nop
    }

    behavior make_behavior() {
      testee_ = spawn<testee, monitored>(this);
      return {
        [=](const down_msg& msg) {
          CAF_CHECK_EQUAL(msg.reason, exit_reason::user_shutdown);
          CAF_CHECK_EQUAL(msg.source, testee_.address());
          if (++downs_ == 2) {
            quit(msg.reason);
          }
        },
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

    void on_exit() {
      testee_ = invalid_actor;
    }

  private:
    int downs_;
    actor testee_;
  };
  actor_system system;
  anon_send(system.spawn<spawner>(), die_atom::value);
}
