// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_registry.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

namespace {

behavior dummy() {
  return {[](int i) { return i; }};
}

WITH_FIXTURE(test::fixture::deterministic) {

TEST("erase") {
  // CAF registers a few actors by itself.
  scoped_actor self{sys};
  auto baseline = sys.registry().named_actors().size();
  sys.registry().put("foo", sys.spawn(dummy));
  check_eq(sys.registry().named_actors().size(), baseline + 1u);
  self->mail(42).send(sys.registry().get<actor>("foo"));
  dispatch_message();
  auto received = std::make_shared<bool>(false);
  self->receive([this, received](int i) {
    *received = true;
    check_eq(i, 42);
  });
  check(*received);
  sys.registry().erase("foo");
  check_eq(sys.registry().named_actors().size(), baseline);
}

TEST("serialization roundtrips go through the registry") {
  auto hdl = sys.spawn(dummy);
  log::test::debug("hdl.id: {}", hdl->id());
  byte_buffer buf;
  binary_serializer sink{sys, buf};
  if (!sink.apply(hdl))
    fail("serialization failed: {}", sink.get_error());
  log::test::debug("buf: {}", buf);
  actor hdl2;
  binary_deserializer source{sys, buf};
  if (!source.apply(hdl2))
    fail("deserialization failed: {}", source.get_error());
  check_eq(hdl, hdl2);
  anon_send_exit(hdl, exit_reason::user_shutdown);
  dispatch_messages();
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
