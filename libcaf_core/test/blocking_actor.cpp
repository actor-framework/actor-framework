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

#define CAF_SUITE blocking_actor
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

struct fixture {
  actor_system_config cfg;
  actor_system system;
  scoped_actor self;

  fixture() : system(cfg), self(system) {
    // nop
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(blocking_actor_tests, fixture)

CAF_TEST(catch_all) {
  self->send(self, 42);
  self->receive(
    [](float) {
      CAF_FAIL("received unexpected float");
    },
    others >> [](message_view& x) -> result<message> {
      CAF_CHECK_EQUAL(to_string(x.content()), "(42)");
      return sec::unexpected_message;
    }
  );
  self->receive(
    [](const error& err) {
      CAF_CHECK_EQUAL(err, sec::unexpected_message);
    }
  );
}

CAF_TEST_FIXTURE_SCOPE_END()
