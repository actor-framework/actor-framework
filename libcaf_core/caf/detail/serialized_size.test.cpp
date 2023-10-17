// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/serialized_size.hpp"

#include "caf/test/test.hpp"

#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"

#include <cstddef>
#include <vector>

using namespace caf;

using caf::detail::serialized_size;

namespace {

struct fixture : test_coordinator_fixture<> {
  template <class... Ts>
  size_t actual_size(const Ts&... xs) {
    byte_buffer buf;
    binary_serializer sink{sys, buf};
    if (!(sink.apply(xs) && ...))
      CAF_FAIL("failed to serialize data: " << sink.get_error());
    return buf.size();
  }
};

#define check_SAME_SIZE(value)                                                 \
  check_eq(serialized_size(value), actual_size(value))

WITH_FIXTURE(fixture) {

TEST("numbers") {
  check_SAME_SIZE(int8_t{42});
  check_SAME_SIZE(int16_t{42});
  check_SAME_SIZE(int32_t{42});
  check_SAME_SIZE(int64_t{42});
  check_SAME_SIZE(uint8_t{42});
  check_SAME_SIZE(uint16_t{42});
  check_SAME_SIZE(uint32_t{42});
  check_SAME_SIZE(uint64_t{42});
  check_SAME_SIZE(4.2f);
  check_SAME_SIZE(4.2);
}

TEST("containers") {
  check_SAME_SIZE(std::string{"foobar"});
  check_SAME_SIZE(std::vector<char>({'a', 'b', 'c'}));
  check_SAME_SIZE(std::vector<std::string>({"hello", "world"}));
}

TEST("messages") {
  check_SAME_SIZE(make_message(42));
  check_SAME_SIZE(make_message(1, 2, 3));
  check_SAME_SIZE(make_message("hello", "world"));
}

} // WITH_FIXTURE(fixture)

} // namespace
