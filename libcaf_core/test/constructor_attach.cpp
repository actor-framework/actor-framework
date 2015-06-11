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
    explicit testee(actor buddy) : buddy_(buddy) {
      attach_functor([=](uint32_t reason) {
        send(buddy_, done_atom::value, reason);
      });
    }
    behavior make_behavior() {
      return {
        [=](die_atom) {
          quit(exit_reason::user_shutdown);
        }
      };
    }
  private:
    actor buddy_;
  };
  class spawner : public event_based_actor {
  public:
    spawner() : downs_(0) {
    }
    behavior make_behavior() {
      testee_ = spawn<testee, monitored>(this);
      return {
        [=](const down_msg& msg) {
          CAF_CHECK_EQUAL(msg.reason, exit_reason::user_shutdown);
          if (++downs_ == 2) {
            quit(msg.reason);
          }
        },
        [=](done_atom, uint32_t reason) {
          CAF_CHECK_EQUAL(reason, exit_reason::user_shutdown);
          if (++downs_ == 2) {
            quit(reason);
          }
        },
        others >> [=] {
          forward_to(testee_);
        }
      };
    }
  private:
    int downs_;
    actor testee_;
  };
  anon_send(spawn<spawner>(), die_atom::value);
  await_all_actors_done();
  shutdown();
}
