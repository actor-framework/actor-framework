// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE optional

#include "caf/optional.hpp"

#include "core-test.hpp"

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

CAF_TEST(empty) {
  optional<int> x;
  optional<int> y;
  CHECK(x == y);
  CHECK(!(x != y));
}

CAF_TEST(equality) {
  optional<int> x = 42;
  optional<int> y = 7;
  CHECK(x != y);
  CHECK(!(x == y));
}

CAF_TEST(ordering) {
  optional<int> x = 42;
  optional<int> y = 7;
  CHECK(x > y);
  CHECK(x >= y);
  CHECK(y < x);
  CHECK(y <= x);
  CHECK(!(y > x));
  CHECK(!(y >= x));
  CHECK(!(x < y));
  CHECK(!(x <= y));
  CHECK(x < 4711);
  CHECK(4711 > x);
  CHECK(4711 >= x);
  CHECK(!(x > 4711));
  CHECK(!(x >= 4711));
  CHECK(!(4211 < x));
  CHECK(!(4211 <= x));
}

CAF_TEST(custom_type_none) {
  optional<qwertz> x;
  CHECK(x == none);
}

CAF_TEST(custom_type_engaged) {
  qwertz obj{1, 2};
  optional<qwertz> x = obj;
  CHECK(x != none);
  CHECK(obj == x);
  CHECK(x == obj);
  CHECK(obj == *x);
  CHECK(*x == obj);
}
