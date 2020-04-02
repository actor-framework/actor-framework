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

#define CAF_SUITE result

#include "core-test.hpp"

#include "caf/sec.hpp"
#include "caf/result.hpp"

using namespace std;
using namespace caf;

namespace {

template<class T>
void test_unit_void() {
  auto x = result<T>{};
  CAF_CHECK(holds_alternative<message>(x));
}

} // namespace anonymous

CAF_TEST(value) {
  auto x = result<int>{42};
  CAF_REQUIRE(holds_alternative<message>(x));
  if (auto view = make_typed_message_view<int>(get<message>(x)))
    CAF_CHECK_EQUAL(get<0>(view), 42);
  else
    CAF_FAIL("unexpected types in result message");
}

CAF_TEST(expected) {
  auto x = result<int>{expected<int>{42}};
  CAF_REQUIRE(holds_alternative<message>(x));
  if (auto view = make_typed_message_view<int>(get<message>(x)))
    CAF_CHECK_EQUAL(get<0>(view), 42);
  else
    CAF_FAIL("unexpected types in result message");
}

CAF_TEST(void_specialization) {
  test_unit_void<void>();
}

CAF_TEST(unit_specialization) {
  test_unit_void<unit_t>();
}
