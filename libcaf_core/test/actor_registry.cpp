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

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"

using namespace caf;

namespace {

behavior dummy() {
  return {[](int i) { return i; }};
}

} // namespace

CAF_TEST_FIXTURE_SCOPE(actor_registry_tests, test_coordinator_fixture<>)

CAF_TEST(erase) {
  // CAF registers a few actors by itself.
  auto baseline = sys.registry().named_actors().size();
  sys.registry().put("foo", sys.spawn(dummy));
  CAF_CHECK_EQUAL(sys.registry().named_actors().size(), baseline + 1u);
  self->send(sys.registry().get<actor>("foo"), 42);
  run();
  expect((int), from(_).to(self).with(42));
  sys.registry().erase("foo");
  CAF_CHECK_EQUAL(sys.registry().named_actors().size(), baseline);
}

CAF_TEST(serialization roundtrips go through the registry) {
  auto hdl = sys.spawn(dummy);
  byte_buffer buf;
  binary_serializer sink{sys, buf};
  if (auto err = sink(hdl))
    CAF_FAIL("serialization failed: " << sys.render(err));
  actor hdl2;
  binary_deserializer source{sys, buf};
  if (auto err = source(hdl2))
    CAF_FAIL("serialization failed: " << sys.render(err));
  CAF_CHECK_EQUAL(hdl, hdl2);
  anon_send_exit(hdl, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
