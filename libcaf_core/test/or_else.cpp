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

#define CAF_SUITE or_else
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

CAF_TEST(test_or_else) {
  {
    scoped_actor self;
    message_handler handle_a {
      on("a") >> [] { return 1; }
    };
    message_handler handle_b {
      on("b") >> [] { return 2; }
    };
    message_handler handle_c {
      on("c") >> [] { return 3; }
    };
    auto run_testee([&](actor testee) {
      self->sync_send(testee, "a").await([](int i) {
        CAF_CHECK_EQUAL(i, 1);
      });
      self->sync_send(testee, "b").await([](int i) {
        CAF_CHECK_EQUAL(i, 2);
      });
      self->sync_send(testee, "c").await([](int i) {
        CAF_CHECK_EQUAL(i, 3);
      });
      self->send_exit(testee, exit_reason::user_shutdown);
      self->await_all_other_actors_done();

    });
    CAF_MESSAGE("run_testee: handle_a.or_else(handle_b).or_else(handle_c)");
    run_testee(
      spawn([=] {
        return handle_a.or_else(handle_b).or_else(handle_c);
      })
    );
    CAF_MESSAGE("run_testee: handle_a.or_else(handle_b), on(\"c\") ...");
    run_testee(
      spawn([=] {
        return handle_a.or_else(handle_b).or_else(on("c") >> [] { return 3; });
      })
    );
    CAF_MESSAGE("run_testee: on(\"a\") ..., handle_b.or_else(handle_c)");
    run_testee(
      spawn([=] {
        return message_handler{on("a") >> [] { return 1; }}.
               or_else(handle_b).or_else(handle_c);
      })
    );
  }
  shutdown();
}
