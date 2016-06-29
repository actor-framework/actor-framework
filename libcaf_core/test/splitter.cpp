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

#define CAF_SUITE splitter
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

#define ERROR_HANDLER                                                          \
  [&](error& err) { CAF_FAIL(system.render(err)); }

using namespace caf;

namespace {

using first_stage = typed_actor<replies_to<double>::with<double, double>>;
using second_stage = typed_actor<replies_to<double, double>::with<double>>;

first_stage::behavior_type typed_first_stage() {
  return [](double x) {
    return std::make_tuple(x * 2.0, x * 4.0);
  };
}

second_stage::behavior_type typed_second_stage() {
  return [](double x, double y) {
    return x * y;
  };
}

behavior untyped_first_stage() {
  return typed_first_stage().unbox();
}

behavior untyped_second_stage() {
  return typed_second_stage().unbox();
}

struct fixture {
  actor_system_config cfg;
  actor_system system;
  scoped_actor self;
  actor first;
  actor second;
  actor first_and_second;

  fixture()
      : system(cfg),
        self(system, true),
        first(unsafe_actor_handle_init),
        second(unsafe_actor_handle_init),
        first_and_second(unsafe_actor_handle_init) {
    // nop
  }

  void init_untyped() {
    using namespace std::placeholders;
    first = system.spawn(untyped_first_stage);
    second = system.spawn(untyped_second_stage);
    first_and_second = splice(first, second.bind(23.0, _1));
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(sequencer_tests, fixture)

CAF_TEST(identity) {
  init_untyped();
  CAF_CHECK_NOT_EQUAL(first, second);
  CAF_CHECK_NOT_EQUAL(first, first_and_second);
  CAF_CHECK_NOT_EQUAL(second, first_and_second);
}

CAF_TEST(kill_first) {
  init_untyped();
  anon_send_exit(first, exit_reason::kill);
  self->wait_for(first_and_second);
}

CAF_TEST(kill_second) {
  init_untyped();
  anon_send_exit(second, exit_reason::kill);
  self->wait_for(first_and_second);
}

CAF_TEST(untyped_splicing) {
  init_untyped();
  self->request(first_and_second, infinite, 42.0).receive(
    [](double x, double y, double z) {
      CAF_CHECK_EQUAL(x, (42.0 * 2.0));
      CAF_CHECK_EQUAL(y, (42.0 * 4.0));
      CAF_CHECK_EQUAL(z, (23.0 * 42.0));
    },
    ERROR_HANDLER
  );
}

CAF_TEST(typed_splicing) {
  using namespace std::placeholders;
  auto stage0 = system.spawn(typed_first_stage);
  auto stage1 = system.spawn(typed_second_stage);
  auto stages = splice(stage0, stage1.bind(23.0, _1));
  using expected_type = typed_actor<replies_to<double>
                                    ::with<double, double, double>>;
  static_assert(std::is_same<decltype(stages), expected_type>::value,
                "splice() did not compute the correct result");
  self->request(stages, infinite, 42.0).receive(
    [](double x, double y, double z) {
      CAF_CHECK_EQUAL(x, (42.0 * 2.0));
      CAF_CHECK_EQUAL(y, (42.0 * 4.0));
      CAF_CHECK_EQUAL(z, (23.0 * 42.0));
    },
    ERROR_HANDLER
  );
  // stage0 and stage1 go out of scope, leaving only the references
  // in stages, which will also go out of scope
}

CAF_TEST_FIXTURE_SCOPE_END()
