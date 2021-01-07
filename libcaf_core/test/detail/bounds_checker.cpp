// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.bounds_checker

#include "caf/detail/bounds_checker.hpp"

#include "caf/test/dsl.hpp"

namespace {

template <class T>
bool check(int64_t x) {
  return caf::detail::bounds_checker<T>::check(x);
}

} // namespace

CAF_TEST(small integers) {
  CAF_CHECK_EQUAL(check<int8_t>(128), false);
  CAF_CHECK_EQUAL(check<int8_t>(127), true);
  CAF_CHECK_EQUAL(check<int8_t>(-128), true);
  CAF_CHECK_EQUAL(check<int8_t>(-129), false);
  CAF_CHECK_EQUAL(check<uint8_t>(-1), false);
  CAF_CHECK_EQUAL(check<uint8_t>(0), true);
  CAF_CHECK_EQUAL(check<uint8_t>(255), true);
  CAF_CHECK_EQUAL(check<uint8_t>(256), false);
  CAF_CHECK_EQUAL(check<int16_t>(-32769), false);
  CAF_CHECK_EQUAL(check<int16_t>(-32768), true);
  CAF_CHECK_EQUAL(check<int16_t>(32767), true);
  CAF_CHECK_EQUAL(check<int16_t>(32768), false);
  CAF_CHECK_EQUAL(check<uint16_t>(-1), false);
  CAF_CHECK_EQUAL(check<uint16_t>(0), true);
  CAF_CHECK_EQUAL(check<uint16_t>(65535), true);
  CAF_CHECK_EQUAL(check<uint16_t>(65536), false);
}

CAF_TEST(large unsigned integers) {
  CAF_CHECK_EQUAL(check<uint64_t>(-1), false);
  CAF_CHECK_EQUAL(check<uint64_t>(0), true);
  CAF_CHECK_EQUAL(check<uint64_t>(std::numeric_limits<int64_t>::max()), true);
}
