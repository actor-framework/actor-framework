// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/exit_reason.hpp"

#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"

using namespace caf;

namespace {

struct fixture {
  template <class T>
  T roundtrip(T input) {
    byte_buffer buf;
    {
      binary_serializer sink{buf};
      if (!sink.apply(input)) {
        test::runnable::current().fail("serialization failed: {}",
                                       sink.get_error());
      }
    }
    binary_deserializer source{buf};
    T output;
    if (!source.apply(output))
      test::runnable::current().fail("deserialization failed: {}",
                                     source.get_error());
    return output;
  }
};

} // namespace

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
