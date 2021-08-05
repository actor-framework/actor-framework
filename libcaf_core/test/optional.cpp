// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE optional

#include "core-test.hpp"

#include "caf/optional.hpp"

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

} // namespace

CAF_PUSH_DEPRECATED_WARNING

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
  CAF_CHECK(x == obj);
  CAF_CHECK(obj == *x);
  CAF_CHECK(*x == obj);
}

CAF_POP_WARNINGS
