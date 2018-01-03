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

#include "caf/config.hpp"

#define CAF_SUITE local_group
#include "caf/test/unit_test.hpp"

#include <array>
#include <chrono>
#include <algorithm>

#include "caf/all.hpp"

using namespace caf;

namespace {

using msg_atom = atom_constant<atom("msg")>;
using timeout_atom = atom_constant<atom("timeout")>;

using testee_if = typed_actor<replies_to<get_atom>::with<int>,
                              reacts_to<put_atom, int>>;

struct testee_state {
  int x = 0;
};

behavior testee_impl(stateful_actor<testee_state>* self) {
  auto subscriptions = self->joined_groups();
  return {
    [=](put_atom, int x) {
      self->state.x = x;
    },
    [=](get_atom) {
      return self->state.x;
    }
  };
};

struct fixture {
  actor_system_config config;
  actor_system system{config};
  scoped_actor self{system};
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(group_tests, fixture)

CAF_TEST(class_based_joined_at_spawn) {
  auto grp = system.groups().get_local("test");
  // initialize all testee actors, spawning them in the group
  std::array<actor, 10> xs;
  for (auto& x : xs)
    x = system.spawn_in_group(grp, testee_impl);
  // get a function view for all testees
  std::array<function_view<testee_if>, 10> fs;
  std::transform(xs.begin(), xs.end(), fs.begin(), [](const actor& x) {
    return make_function_view(actor_cast<testee_if>(x));
  });
  // make sure all actors start at 0
  for (auto& f : fs)
    CAF_CHECK_EQUAL(f(get_atom::value), 0);
  // send a put to all actors via the group and make sure they change state
  self->send(grp, put_atom::value, 42);
  for (auto& f : fs)
    CAF_CHECK_EQUAL(f(get_atom::value), 42);
  // shutdown all actors
  for (auto& x : xs)
    self->send_exit(x, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()

