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

#define CAF_SUITE simple_timeout
#include "caf/test/unit_test.hpp"

#include <chrono>
#include <memory>

#include "caf/all.hpp"

using namespace caf;

namespace {

using reset_atom = atom_constant<atom("reset")>;
using timer = typed_actor<reacts_to<reset_atom>>;

timer::behavior_type timer_impl(timer::pointer self) {
  auto had_reset = std::make_shared<bool>(false);
  self->delayed_send(self, std::chrono::milliseconds(100), reset_atom::value);
  return {
    [=](reset_atom) {
      CAF_MESSAGE("timer reset");
      *had_reset = true;
    },
    after(std::chrono::milliseconds(600)) >> [=] {
      CAF_MESSAGE("timer expired");
      CAF_REQUIRE(*had_reset);
      self->quit();
    }
  };
}

struct fixture {
  ~fixture() {
    await_all_actors_done();
    shutdown();
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(timeout_tests, fixture)

CAF_TEST(single_timeout) {
  spawn(timer_impl);
}

CAF_TEST_FIXTURE_SCOPE_END()
