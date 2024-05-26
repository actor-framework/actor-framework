// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/exit_reason.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"

using namespace caf;

namespace {

struct fixture {
  actor_system_config cfg;
  actor_system sys{cfg};

  template <class T>
  T roundtrip(T x) {
    byte_buffer buf;
    binary_serializer sink(sys, buf);
    if (!sink.apply(x))
      test::runnable::current().fail("serialization failed: {}",
                                     sink.get_error());
    binary_deserializer source(sys, make_span(buf));
    T y;
    if (!source.apply(y))
      test::runnable::current().fail("deserialization failed: {}",
                                     source.get_error());
    return y;
  }
};

#define CHECK_SERIALIZATION(error_code)                                        \
  check_eq(error_code, roundtrip(error_code))

WITH_FIXTURE(fixture) {

TEST("exit_reason values are serializable") {
  CHECK_SERIALIZATION(exit_reason::kill);
  CHECK_SERIALIZATION(exit_reason::normal);
  CHECK_SERIALIZATION(exit_reason::out_of_workers);
  CHECK_SERIALIZATION(exit_reason::remote_link_unreachable);
  CHECK_SERIALIZATION(exit_reason::unreachable);
  CHECK_SERIALIZATION(exit_reason::unknown);
  CHECK_SERIALIZATION(exit_reason::user_shutdown);
}

} // WITH_FIXTURE(fixture)

} // namespace
