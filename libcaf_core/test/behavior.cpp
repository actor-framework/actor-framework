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

#define CAF_SUITE behavior

#include "caf/config.hpp"

#include "caf/test/unit_test.hpp"

#include <functional>

#include "caf/send.hpp"
#include "caf/behavior.hpp"
#include "caf/actor_system.hpp"
#include "caf/message_handler.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/make_type_erased_tuple_view.hpp"

using namespace caf;
using namespace std;

using hi_atom = atom_constant<atom("hi")>;
using ho_atom = atom_constant<atom("ho")>;

namespace {

class nocopy_fun {
public:
  nocopy_fun() = default;

  nocopy_fun(nocopy_fun&&) = default;

  nocopy_fun& operator=(nocopy_fun&&) = default;

  nocopy_fun(const nocopy_fun&) = delete;

  nocopy_fun& operator=(const nocopy_fun&) = delete;

  int operator()(int x, int y) {
    return x + y;
  }
};

struct fixture {
  message m1 = make_message(1);
  message m2 = make_message(1, 2);
  message m3 = make_message(1, 2, 3);
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(behavior_tests, fixture)

CAF_TEST(default_construct) {
  behavior f;
  CAF_CHECK_EQUAL(f(m1), none);
  CAF_CHECK_EQUAL(f(m2), none);
  CAF_CHECK_EQUAL(f(m3), none);
}

CAF_TEST(nocopy_function_object) {
  behavior f{nocopy_fun{}};
  CAF_CHECK_EQUAL(f(m1), none);
  CAF_CHECK_EQUAL(to_string(f(m2)), "*(3)");
  CAF_CHECK_EQUAL(f(m3), none);
}

CAF_TEST(single_lambda_construct) {
  behavior f{[](int x) { return x + 1; }};
  CAF_CHECK_EQUAL(to_string(f(m1)), "*(2)");
  CAF_CHECK_EQUAL(f(m2), none);
  CAF_CHECK_EQUAL(f(m3), none);
}

CAF_TEST(multiple_lambda_construct) {
  behavior f{
    [](int x) { return x + 1; },
    [](int x, int y) { return x * y; }
  };
  CAF_CHECK_EQUAL(to_string(f(m1)), "*(2)");
  CAF_CHECK_EQUAL(to_string(f(m2)), "*(2)");
  CAF_CHECK_EQUAL(f(m3), none);
}

CAF_TEST(become_empty_behavior) {
  actor_system_config cfg{};
  actor_system sys{cfg};
  auto make_bhvr = [](event_based_actor* self) -> behavior {
    return {
      [=](int) { self->become(behavior{}); }
    };
  };
  anon_send(sys.spawn(make_bhvr), int{5});
}

CAF_TEST_FIXTURE_SCOPE_END()
