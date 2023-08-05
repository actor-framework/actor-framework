// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE actor_registry

#include "caf/actor_registry.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

behavior dummy() {
  return {[](int i) { return i; }};
}

} // namespace

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

CAF_TEST(erase) {
  // CAF registers a few actors by itself.
  auto baseline = sys.registry().named_actors().size();
  sys.registry().put("foo", sys.spawn(dummy));
  CHECK_EQ(sys.registry().named_actors().size(), baseline + 1u);
  self->send(sys.registry().get<actor>("foo"), 42);
  run();
  expect((int), from(_).to(self).with(42));
  sys.registry().erase("foo");
  CHECK_EQ(sys.registry().named_actors().size(), baseline);
}

CAF_TEST(serialization roundtrips go through the registry) {
  auto hdl = sys.spawn(dummy);
  MESSAGE("hdl.id: " << hdl->id());
  byte_buffer buf;
  binary_serializer sink{sys, buf};
  if (!sink.apply(hdl))
    CAF_FAIL("serialization failed: " << sink.get_error());
  MESSAGE("buf: " << buf);
  actor hdl2;
  binary_deserializer source{sys, buf};
  if (!source.apply(hdl2))
    CAF_FAIL("deserialization failed: " << source.get_error());
  CHECK_EQ(hdl, hdl2);
  anon_send_exit(hdl, exit_reason::user_shutdown);
}

END_FIXTURE_SCOPE()
