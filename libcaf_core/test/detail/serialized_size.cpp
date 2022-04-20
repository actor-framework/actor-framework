// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.serialized_size

#include "caf/detail/serialized_size.hpp"

#include "core-test.hpp"

#include <vector>

#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"

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

} // namespace

#define CHECK_SAME_SIZE(value)                                                 \
  CHECK_EQ(serialized_size(value), actual_size(value))

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(numbers) {
  CHECK_SAME_SIZE(int8_t{42});
  CHECK_SAME_SIZE(int16_t{42});
  CHECK_SAME_SIZE(int32_t{42});
  CHECK_SAME_SIZE(int64_t{42});
  CHECK_SAME_SIZE(uint8_t{42});
  CHECK_SAME_SIZE(uint16_t{42});
  CHECK_SAME_SIZE(uint32_t{42});
  CHECK_SAME_SIZE(uint64_t{42});
  CHECK_SAME_SIZE(4.2f);
  CHECK_SAME_SIZE(4.2);
}

CAF_TEST(containers) {
  CHECK_SAME_SIZE(std::string{"foobar"});
  CHECK_SAME_SIZE(std::vector<char>({'a', 'b', 'c'}));
  CHECK_SAME_SIZE(std::vector<std::string>({"hello", "world"}));
}

CAF_TEST(messages) {
  CHECK_SAME_SIZE(make_message(42));
  CHECK_SAME_SIZE(make_message(1, 2, 3));
  CHECK_SAME_SIZE(make_message("hello", "world"));
}

END_FIXTURE_SCOPE()
