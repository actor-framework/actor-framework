// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.bounds_checker

#include "caf/detail/bounds_checker.hpp"

#include "core-test.hpp"

namespace {

template <class T, class U>
bool check(U x) {
  return caf::detail::bounds_checker<T>::check(x);
}

} // namespace

CAF_TEST(small integers) {
  CHECK_EQ(check<int8_t>(128), false);
  CHECK_EQ(check<int8_t>(127), true);
  CHECK_EQ(check<int8_t>(-128), true);
  CHECK_EQ(check<int8_t>(-129), false);
  CHECK_EQ(check<uint8_t>(-1), false);
  CHECK_EQ(check<uint8_t>(0), true);
  CHECK_EQ(check<uint8_t>(255), true);
  CHECK_EQ(check<uint8_t>(256), false);
  CHECK_EQ(check<int16_t>(-32769), false);
  CHECK_EQ(check<int16_t>(-32768), true);
  CHECK_EQ(check<int16_t>(32767), true);
  CHECK_EQ(check<int16_t>(32768), false);
  CHECK_EQ(check<uint16_t>(-1), false);
  CHECK_EQ(check<uint16_t>(0), true);
  CHECK_EQ(check<uint16_t>(65535), true);
  CHECK_EQ(check<uint16_t>(65536), false);
}

CAF_TEST(large unsigned integers) {
  CHECK_EQ(check<uint64_t>(-1), false);
  CHECK_EQ(check<uint64_t>(0), true);
  CHECK_EQ(check<uint64_t>(0u), true);
  CHECK_EQ(check<uint64_t>(std::numeric_limits<int64_t>::max()), true);
  CHECK_EQ(check<uint64_t>(std::numeric_limits<uint64_t>::max()), true);
}
