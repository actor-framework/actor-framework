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

#define CAF_SUITE delayed_send

#include <chrono>

#include "caf/actor_system.hpp"
#include "caf/behavior.hpp"
#include "caf/event_based_actor.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

using std::chrono::seconds;

namespace {

behavior testee_impl(event_based_actor* self) {
  self->set_default_handler(drop);
  return {
    [] {
      // nop
    }
  };
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(request_timeout_tests, test_coordinator_fixture<>)

CAF_TEST(delayed actor message) {
  auto testee = sys.spawn(testee_impl);
  self->delayed_send(testee, seconds(1), "hello world");
  sched.trigger_timeout();
  expect((std::string), from(self).to(testee).with("hello world"));
}

CAF_TEST(delayed group message) {
  auto grp = sys.groups().anonymous();
  auto testee = sys.spawn_in_group(grp, testee_impl);
  self->delayed_send(grp, seconds(1), "hello world");
  sched.trigger_timeout();
  expect((std::string), from(self).to(testee).with("hello world"));
  // The group keeps a reference, so we need to shutdown 'manually'.
  anon_send_exit(testee, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
