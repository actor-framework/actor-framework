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

#define CAF_SUITE result
#include "caf/test/unit_test.hpp"

#include "caf/sec.hpp"
#include "caf/result.hpp"

using namespace std;
using namespace caf;

namespace {

template<class T>
void test_unit_void() {
  auto x = result<T>{};
  CAF_CHECK_EQUAL(x.flag, rt_value);
  x = skip();
  CAF_CHECK_EQUAL(x.flag, rt_skip);
  x = expected<T>{};
  CAF_CHECK_EQUAL(x.flag, rt_value);
  x = expected<T>{sec::unexpected_message};
  CAF_CHECK_EQUAL(x.flag, rt_error);
  CAF_CHECK_EQUAL(x.err, make_error(sec::unexpected_message));
}

} // namespace anonymous

CAF_TEST(skip) {
  auto x = result<>{skip()};
  CAF_CHECK_EQUAL(x.flag, rt_skip);
  CAF_CHECK(x.value.empty());
}

CAF_TEST(value) {
  auto x = result<int>{42};
  CAF_CHECK_EQUAL(x.flag, rt_value);
  CAF_CHECK_EQUAL(x.value.get_as<int>(0), 42);
}

CAF_TEST(expected) {
  auto x = result<int>{expected<int>{42}};
  CAF_CHECK_EQUAL(x.flag, rt_value);
  CAF_CHECK_EQUAL(x.value.get_as<int>(0), 42);
  x = expected<int>{sec::unexpected_message};
  CAF_CHECK_EQUAL(x.flag, rt_error);
  CAF_CHECK_EQUAL(x.err, make_error(sec::unexpected_message));
  CAF_CHECK(x.value.empty());
}

CAF_TEST(void_specialization) {
  test_unit_void<void>();
}

CAF_TEST(unit_specialization) {
  test_unit_void<unit_t>();
}
