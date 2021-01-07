// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
