// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/pec.hpp"

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

TEST("pec values are serializable") {
  CHECK_SERIALIZATION(pec::exponent_overflow);
  CHECK_SERIALIZATION(pec::exponent_underflow);
  CHECK_SERIALIZATION(pec::fractional_timespan);
  CHECK_SERIALIZATION(pec::integer_overflow);
  CHECK_SERIALIZATION(pec::integer_underflow);
  CHECK_SERIALIZATION(pec::invalid_argument);
  CHECK_SERIALIZATION(pec::invalid_category);
  CHECK_SERIALIZATION(pec::invalid_escape_sequence);
  CHECK_SERIALIZATION(pec::invalid_field_name);
  CHECK_SERIALIZATION(pec::invalid_range_expression);
  CHECK_SERIALIZATION(pec::invalid_state);
  CHECK_SERIALIZATION(pec::missing_argument);
  CHECK_SERIALIZATION(pec::missing_field);
  CHECK_SERIALIZATION(pec::nested_too_deeply);
  CHECK_SERIALIZATION(pec::not_an_option);
  CHECK_SERIALIZATION(pec::repeated_field_name);
  CHECK_SERIALIZATION(pec::success);
  CHECK_SERIALIZATION(pec::type_mismatch);
  CHECK_SERIALIZATION(pec::trailing_character);
  CHECK_SERIALIZATION(pec::too_many_characters);
  CHECK_SERIALIZATION(pec::unexpected_character);
  CHECK_SERIALIZATION(pec::unexpected_eof);
  CHECK_SERIALIZATION(pec::unexpected_newline);
}

} // WITH_FIXTURE(fixture)

} // namespace
