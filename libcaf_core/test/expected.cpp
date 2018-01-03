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

#define CAF_SUITE expected
#include "caf/test/unit_test.hpp"

#include "caf/sec.hpp"
#include "caf/expected.hpp"

using namespace std;
using namespace caf;

#define CHECK(x) CAF_CHECK(x);

#define CHECK_EQ(x, y)                                                         \
  CAF_CHECK(x == y);                                                           \
  CAF_CHECK(y == x);

#define CHECK_NEQ(x, y)                                                        \
  CAF_CHECK(x != y);                                                           \
  CAF_CHECK(y != x);

namespace {

using e_int = expected<int>;
using e_str = expected<std::string>;

} // namespace <anonymous>

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
  CHECK_NEQ(x, y);
  CHECK_NEQ(x, sec::unexpected_message);
  CHECK_NEQ(y, sec::unexpected_message);
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
  CHECK_NEQ(x, sec::unexpected_message);
  CHECK_NEQ(x, y);
  CHECK_NEQ(y, 42);
  CHECK_NEQ(y, sec::unsupported_sys_key);
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
  CHECK_NEQ(x, sec::unsupported_sys_key);
  CHECK_NEQ(y, sec::unsupported_sys_key);
}

CAF_TEST(move_and_copy) {
  e_str x{sec::unexpected_message};
  e_str y{"hello"};
  x = "hello";
  CHECK_NEQ(x, sec::unexpected_message);
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
  CHECK_NEQ(z, z_cpy);
  CHECK_EQ(z, sec::unsupported_sys_key);
}

CAF_TEST(construction_with_none) {
  e_int x{none};
  CHECK(!x);
  CHECK(!x.error());
}

CAF_TEST(construction_with_no_error) {
  e_int x{no_error};
  CHECK(!x);
  CHECK(!x.error());
  auto f = []() -> e_int {
    return no_error;
  };
  CHECK_EQ(f(), x);
}
