// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/bounds_checker.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include <cstdint>

template <class T, class U>
bool bounds_check(U x) {
  return caf::detail::bounds_checker<T>::check(x);
}

TEST("small integers") {
  check_eq(bounds_check<int8_t>(128), false);
  check_eq(bounds_check<int8_t>(127), true);
  check_eq(bounds_check<int8_t>(-128), true);
  check_eq(bounds_check<int8_t>(-129), false);
  check_eq(bounds_check<uint8_t>(-1), false);
  check_eq(bounds_check<uint8_t>(0), true);
  check_eq(bounds_check<uint8_t>(255), true);
  check_eq(bounds_check<uint8_t>(256), false);
  check_eq(bounds_check<int16_t>(-32769), false);
  check_eq(bounds_check<int16_t>(-32768), true);
  check_eq(bounds_check<int16_t>(32767), true);
  check_eq(bounds_check<int16_t>(32768), false);
  check_eq(bounds_check<uint16_t>(-1), false);
  check_eq(bounds_check<uint16_t>(0), true);
  check_eq(bounds_check<uint16_t>(65535), true);
  check_eq(bounds_check<uint16_t>(65536), false);
}

TEST("large unsigned integers") {
  check_eq(bounds_check<uint64_t>(-1), false);
  check_eq(bounds_check<uint64_t>(0), true);
  check_eq(bounds_check<uint64_t>(0u), true);
  check_eq(bounds_check<uint64_t>(std::numeric_limits<int64_t>::max()), true);
  check_eq(bounds_check<uint64_t>(std::numeric_limits<uint64_t>::max()), true);
}

CAF_TEST_MAIN()
