/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE bounds_checker
#include "caf/test/dsl.hpp"

#include "caf/detail/bounds_checker.hpp"

namespace {

template <class T>
bool check(int64_t x) {
  return caf::detail::bounds_checker<T>::check(x);
}

} // namespace <anonymous>

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
