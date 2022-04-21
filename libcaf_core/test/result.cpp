// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE result

#include "caf/result.hpp"

#include "core-test.hpp"

#include "caf/sec.hpp"

using namespace caf;

namespace {

template <class T>
void test_unit_void() {
  auto x = result<T>{};
  CHECK(holds_alternative<message>(x));
}

} // namespace

CAF_TEST(value) {
  auto x = result<int>{42};
  REQUIRE(holds_alternative<message>(x));
  if (auto view = make_typed_message_view<int>(get<message>(x)))
    CHECK_EQ(get<0>(view), 42);
  else
    FAIL("unexpected types in result message");
}

CAF_TEST(expected) {
  auto x = result<int>{expected<int>{42}};
  REQUIRE(holds_alternative<message>(x));
  if (auto view = make_typed_message_view<int>(get<message>(x)))
    CHECK_EQ(get<0>(view), 42);
  else
    FAIL("unexpected types in result message");
}

CAF_TEST(void_specialization) {
  test_unit_void<void>();
}

CAF_TEST(unit_specialization) {
  test_unit_void<unit_t>();
}
