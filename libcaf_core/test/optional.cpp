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

#define CAF_SUITE optional
#include "caf/test/unit_test.hpp"

#include "caf/optional.hpp"

using namespace std;
using namespace caf;

namespace {

struct qwertz {
  qwertz(int x, int y) : x_(x), y_(y) {
    // nop
  }
  int x_;
  int y_;
};

bool operator==(const qwertz& lhs, const qwertz& rhs) {
  return lhs.x_ == rhs.x_ && lhs.y_ == rhs.y_;
}

} // namespace <anonymous>

CAF_TEST(empty) {
  optional<int> x;
  optional<int> y;
  CAF_CHECK(x == y);
  CAF_CHECK(!(x != y));
}

CAF_TEST(equality) {
  optional<int> x = 42;
  optional<int> y = 7;
  CAF_CHECK(x != y);
  CAF_CHECK(!(x == y));
}

CAF_TEST(ordering) {
  optional<int> x = 42;
  optional<int> y = 7;
  CAF_CHECK(x > y);
  CAF_CHECK(x >= y);
  CAF_CHECK(y < x);
  CAF_CHECK(y <= x);
  CAF_CHECK(!(y > x));
  CAF_CHECK(!(y >= x));
  CAF_CHECK(!(x < y));
  CAF_CHECK(!(x <= y));
  CAF_CHECK(x < 4711);
  CAF_CHECK(4711 > x);
  CAF_CHECK(4711 >= x);
  CAF_CHECK(!(x > 4711));
  CAF_CHECK(!(x >= 4711));
  CAF_CHECK(!(4211 < x));
  CAF_CHECK(!(4211 <= x));
}

CAF_TEST(custom_type_none) {
  optional<qwertz> x;
  CAF_CHECK(x == none);
}

CAF_TEST(custom_type_engaged) {
  qwertz obj{1, 2};
  optional<qwertz> x = obj;
  CAF_CHECK(x != none);
  CAF_CHECK(obj == x);
  CAF_CHECK(x == obj );
  CAF_CHECK(obj == *x);
  CAF_CHECK(*x == obj);
}
