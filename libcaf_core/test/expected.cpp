// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE expected

#include "caf/expected.hpp"

#include "core-test.hpp"

#include "caf/sec.hpp"

using namespace caf;

namespace {

using e_int = expected<int>;
using e_str = expected<std::string>;

} // namespace

CAF_TEST(both_engaged_equal) {
  e_int x{42};
  e_int y{42};
  CHECK(x);
  CHECK(y);
  CHECK_EQ(x, y);
  CHECK_EQ(x, 42);
  CHECK_EQ(y, 42);
}

CAF_TEST(both_engaged_not_equal) {
  e_int x{42};
  e_int y{24};
  CHECK(x);
  CHECK(y);
  CHECK_NE(x, y);
  CHECK_NE(x, sec::unexpected_message);
  CHECK_NE(y, sec::unexpected_message);
  CHECK_EQ(x, 42);
  CHECK_EQ(y, 24);
}

CAF_TEST(engaged_plus_not_engaged) {
  e_int x{42};
  e_int y{sec::unexpected_message};
  CHECK(x);
  CHECK(!y);
  CHECK_EQ(x, 42);
  CHECK_EQ(y, sec::unexpected_message);
  CHECK_NE(x, sec::unexpected_message);
  CHECK_NE(x, y);
  CHECK_NE(y, 42);
  CHECK_NE(y, sec::unsupported_sys_key);
}

CAF_TEST(both_not_engaged) {
  e_int x{sec::unexpected_message};
  e_int y{sec::unexpected_message};
  CHECK(!x);
  CHECK(!y);
  CHECK_EQ(x, y);
  CHECK_EQ(x, sec::unexpected_message);
  CHECK_EQ(y, sec::unexpected_message);
  CHECK_EQ(x.error(), y.error());
  CHECK_NE(x, sec::unsupported_sys_key);
  CHECK_NE(y, sec::unsupported_sys_key);
}

CAF_TEST(move_and_copy) {
  e_str x{sec::unexpected_message};
  e_str y{"hello"};
  x = "hello";
  CHECK_NE(x, sec::unexpected_message);
  CHECK_EQ(x, "hello");
  CHECK_EQ(x, y);
  y = "world";
  x = std::move(y);
  CHECK_EQ(x, "world");
  e_str z{std::move(x)};
  CHECK_EQ(z, "world");
  e_str z_cpy{z};
  CHECK_EQ(z_cpy, "world");
  CHECK_EQ(z, z_cpy);
  z = e_str{sec::unsupported_sys_key};
  CHECK_NE(z, z_cpy);
  CHECK_EQ(z, sec::unsupported_sys_key);
}

CAF_TEST(construction_with_none) {
  e_int x{none};
  CHECK(!x);
  CHECK(!x.error());
}
