// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/message_id.hpp"

#include "caf/test/test.hpp"

using namespace caf;

TEST("default construction") {
  message_id x;
  check_eq(x.is_async(), true);
  check_eq(x.is_request(), false);
  check_eq(x.is_response(), false);
  check_eq(x.is_answered(), false);
  check(x.category() == message_id::normal_message_category);
  check_eq(x.is_urgent_message(), false);
  check_eq(x.is_normal_message(), true);
  check_eq(x, x.response_id());
  check_eq(x.request_id().integer_value(), 0u);
  check(x.integer_value() == message_id::default_async_value);
}

TEST("make_message_id") {
  auto x = make_message_id();
  message_id y;
  check_eq(x, y);
  check_eq(x.integer_value(), y.integer_value());
}

TEST("from integer value") {
  auto x = make_message_id(42);
  check_eq(x.is_async(), false);
  check_eq(x.is_request(), true);
  check_eq(x.is_response(), false);
  check_eq(x.is_answered(), false);
  check(x.category() == message_id::normal_message_category);
  check_eq(x.is_urgent_message(), false);
  check_eq(x.is_normal_message(), true);
  check_eq(x.request_id().integer_value(), 42u);
}

TEST("response ID") {
  auto x = make_message_id(42).response_id();
  check_eq(x.is_async(), false);
  check_eq(x.is_request(), false);
  check_eq(x.is_response(), true);
  check_eq(x.is_answered(), false);
  check(x.category() == message_id::normal_message_category);
  check_eq(x.is_urgent_message(), false);
  check_eq(x.is_normal_message(), true);
  check_eq(x.request_id().integer_value(), 42u);
}

TEST("request with high priority") {
  auto x = make_message_id(42).response_id();
  check_eq(x.is_async(), false);
  check_eq(x.is_request(), false);
  check_eq(x.is_response(), true);
  check_eq(x.is_answered(), false);
  check(x.category() == message_id::normal_message_category);
  check_eq(x.is_urgent_message(), false);
  check_eq(x.is_normal_message(), true);
  check_eq(x.request_id().integer_value(), 42u);
}

TEST("with_category") {
  auto x = make_message_id();
  check(x.category() == message_id::normal_message_category);
  for (auto category : {message_id::urgent_message_category,
                        message_id::normal_message_category}) {
    x = x.with_category(category);
    check_eq(x.category(), category);
    check_eq(x.is_request(), false);
    check_eq(x.is_response(), false);
    check_eq(x.is_answered(), false);
  }
}

TEST("comparison") {
  SECTION("sync < async") {
    const auto lhs = message_id{1}; // a sync ID
    const auto rhs = message_id{};  // an async ID (default_async_value)
    check_lt(lhs, rhs);
    check_lt(lhs.integer_value(), rhs.integer_value());
  }
  SECTION("request < response") {
    const auto lhs = message_id{1};
    const auto rhs = lhs.response_id();
    check_lt(lhs, rhs);
    check_lt(lhs.integer_value(), rhs.integer_value());
  }
  SECTION("high-priority async < response") {
    // Response IDs set the high bit. Order follows the unsigned IDs.
    const auto lhs = message_id{0};
    const auto rhs = message_id{1}.response_id();
    check_lt(lhs, rhs);
    check_lt(lhs.integer_value(), rhs.integer_value());
  }
}
