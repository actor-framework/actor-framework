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

#define CAF_SUITE shutdown
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

behavior testee() {
  return {
    [] {
      // nop
    }
  };
}

} // namespace <anonymous>

CAF_TEST(repeated_shutdown) {
  actor_system system;
  for (auto i = 0; i < 10; ++i) {
    CAF_MESSAGE("run #" << i);
    auto g = system.groups().anonymous();
    for (auto j = 0; j < 10; ++j)
      system.spawn_in_group(g, testee);
    anon_send(g, "hello actors");
    anon_send(g, exit_msg{invalid_actor_addr, exit_reason::user_shutdown});
    system.await_all_actors_done();
  }
}
