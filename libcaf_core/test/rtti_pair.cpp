/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE rtti_pair

#include "caf/rtti_pair.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

namespace {

struct foo {
  int x;
  int y;
};

} // namespace <anonymous>

CAF_TEST(make_rtti_pair) {
  auto n = type_nr<int32_t>::value;
  CAF_REQUIRE_NOT_EQUAL(n, 0u);
  auto x1 = make_rtti_pair<int32_t>();
  CAF_CHECK_EQUAL(x1.first, n);
  CAF_CHECK_EQUAL(x1.second, nullptr);
  auto x2 = make_rtti_pair<foo>();
  CAF_CHECK_EQUAL(x2.first, 0);
  CAF_CHECK_EQUAL(x2.second, &typeid(foo));
}

CAF_TEST(to_string) {
  auto n = type_nr<int32_t>::value;
  CAF_CHECK_EQUAL(to_string(make_rtti_pair<int32_t>()),
                  "(" + std::to_string(n) + ", <null>)");
}
