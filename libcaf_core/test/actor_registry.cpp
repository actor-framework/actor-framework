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

#define CAF_SUITE actor_registry

#include "caf/actor_registry.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

namespace {

behavior dummy() {
  return {
    [](int i) {
      return i;
    }
  };
}

using foo_atom = atom_constant<atom("foo")>;

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(actor_registry_tests, test_coordinator_fixture<>)

CAF_TEST(erase) {
  // CAF registers a few actors by itself.
  auto baseline = sys.registry().named_actors().size();
  sys.registry().put(foo_atom::value, sys.spawn(dummy));
  CAF_CHECK_EQUAL(sys.registry().named_actors().size(), baseline + 1u);
  self->send(sys.registry().get<actor>(foo_atom::value), 42);
  run();
  expect((int), from(_).to(self).with(42));
  sys.registry().erase(foo_atom::value);
  CAF_CHECK_EQUAL(sys.registry().named_actors().size(), baseline);
}

CAF_TEST_FIXTURE_SCOPE_END()
