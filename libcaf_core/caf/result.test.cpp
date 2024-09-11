// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/result.hpp"

#include "caf/test/test.hpp"

#include "caf/sec.hpp"
#include "caf/typed_message_view.hpp"

using namespace caf;

TEST("value") {
  auto x = result<int>{42};
  require(holds_alternative<message>(x));
  if (auto view = make_typed_message_view<int>(get<message>(x)))
    check_eq(get<0>(view), 42);
  else
    fail("unexpected types in result message");
}

TEST("expected") {
  auto x = result<int>{expected<int>{42}};
  require(holds_alternative<message>(x));
  if (auto view = make_typed_message_view<int>(get<message>(x)))
    check_eq(get<0>(view), 42);
  else
    fail("unexpected types in result message");
}

TEST("void_specialization") {
  auto x = result<void>{};
  check(holds_alternative<message>(x));
}

TEST("unit_specialization") {
  auto x = result<unit_t>{};
  check(holds_alternative<message>(x));
}
