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

CAF_TEST(test_constructor_attach) {
  class testee : public event_based_actor {
   public:
    testee(actor buddy) : m_buddy(buddy) {
      attach_functor([=](uint32_t reason) {
        send(m_buddy, atom("done"), reason);
      });
    }
    behavior make_behavior() {
      return {
        on(atom("die")) >> [=] {
          quit(exit_reason::user_shutdown);
        }
      };
    }
   private:
    actor m_buddy;
  };
  class spawner : public event_based_actor {
   public:
    spawner() : m_downs(0) {
    }
    behavior make_behavior() {
      m_testee = spawn<testee, monitored>(this);
      return {
        [=](const down_msg& msg) {
          CAF_CHECK_EQUAL(msg.reason, exit_reason::user_shutdown);
          if (++m_downs == 2) {
            quit(msg.reason);
          }
        },
        on(atom("done"), arg_match) >> [=](uint32_t reason) {
          CAF_CHECK_EQUAL(reason, exit_reason::user_shutdown);
          if (++m_downs == 2) {
            quit(reason);
          }
        },
        others >> [=] {
          forward_to(m_testee);
        }
      };
    }
   private:
    int m_downs;
    actor m_testee;
  };
  anon_send(spawn<spawner>(), atom("die"));
  await_all_actors_done();
  shutdown();
}
