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
  actor_system system;
  scoped_actor self{system, true};

  actor first;
  actor second;
  actor first_and_second;

  ~fixture() {
    anon_send_exit(first, exit_reason::kill);
    anon_send_exit(second, exit_reason::kill);
  }

  void init_untyped() {
    using namespace std::placeholders;
    first = system.spawn(untyped_first_stage);
    second = system.spawn(untyped_second_stage);
    first_and_second = splice(first, second.bind(23.0, _1));
  }

  void await_down(const actor& x) {
    self->monitor(x);
    self->receive(
      [&](const down_msg& dm) -> maybe<skip_message_t> {
        if (dm.source != x)
          return skip_message();
        return none;
      }
    );
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(sequencer_tests, fixture)

CAF_TEST(identity) {
  init_untyped();
  CAF_CHECK(first != second);
  CAF_CHECK(first != first_and_second);
  CAF_CHECK(second != first_and_second);
}

CAF_TEST(kill_first) {
  init_untyped();
  anon_send_exit(first, exit_reason::kill);
  await_down(first_and_second);
}

CAF_TEST(kill_second) {
  init_untyped();
  anon_send_exit(second, exit_reason::kill);
  await_down(first_and_second);
}

CAF_TEST(untyped_splicing) {
  init_untyped();
  self->request(first_and_second, indefinite, 42.0).receive(
    [](double x, double y, double z) {
      CAF_CHECK(x == (42.0 * 2.0));
      CAF_CHECK(y == (42.0 * 4.0));
      CAF_CHECK(z == (23.0 * 42.0));
    }
  );
}

CAF_TEST(typed_splicing) {
  using namespace std::placeholders;
  auto x = system.spawn(typed_first_stage);
  auto y = system.spawn(typed_second_stage);
  auto x_and_y = splice(x, y.bind(23.0, _1));
  using expected_type = typed_actor<replies_to<double>
                                    ::with<double, double, double>>;
  static_assert(std::is_same<decltype(x_and_y), expected_type>::value,
                "splice() did not compute the correct result");
  self->request(x_and_y, indefinite, 42.0).receive(
    [](double x, double y, double z) {
      CAF_CHECK(x == (42.0 * 2.0));
      CAF_CHECK(y == (42.0 * 4.0));
      CAF_CHECK(z == (23.0 * 42.0));
    }
  );
  anon_send_exit(x, exit_reason::kill);
  anon_send_exit(y, exit_reason::kill);
}

CAF_TEST_FIXTURE_SCOPE_END()
