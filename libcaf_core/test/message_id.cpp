// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE message_id

#include "caf/message_id.hpp"

#include "core-test.hpp"

using namespace caf;

CAF_TEST(default construction) {
  message_id x;
  CAF_CHECK_EQUAL(x.is_async(), true);
  CAF_CHECK_EQUAL(x.is_request(), false);
  CAF_CHECK_EQUAL(x.is_response(), false);
  CAF_CHECK_EQUAL(x.is_answered(), false);
  CAF_CHECK(x.category() == message_id::normal_message_category);
  CAF_CHECK_EQUAL(x.is_urgent_message(), false);
  CAF_CHECK_EQUAL(x.is_normal_message(), true);
  CAF_CHECK_EQUAL(x, x.response_id());
  CAF_CHECK_EQUAL(x.request_id().integer_value(), 0u);
  CAF_CHECK(x.integer_value() == message_id::default_async_value);
}

CAF_TEST(make_message_id) {
  auto x = make_message_id();
  message_id y;
  CAF_CHECK_EQUAL(x, y);
  CAF_CHECK_EQUAL(x.integer_value(), y.integer_value());
}

CAF_TEST(from integer value) {
  auto x = make_message_id(42);
  CAF_CHECK_EQUAL(x.is_async(), false);
  CAF_CHECK_EQUAL(x.is_request(), true);
  CAF_CHECK_EQUAL(x.is_response(), false);
  CAF_CHECK_EQUAL(x.is_answered(), false);
  CAF_CHECK(x.category() == message_id::normal_message_category);
  CAF_CHECK_EQUAL(x.is_urgent_message(), false);
  CAF_CHECK_EQUAL(x.is_normal_message(), true);
  CAF_CHECK_EQUAL(x.request_id().integer_value(), 42u);
}

CAF_TEST(response ID) {
  auto x = make_message_id(42).response_id();
  CAF_CHECK_EQUAL(x.is_async(), false);
  CAF_CHECK_EQUAL(x.is_request(), false);
  CAF_CHECK_EQUAL(x.is_response(), true);
  CAF_CHECK_EQUAL(x.is_answered(), false);
  CAF_CHECK(x.category() == message_id::normal_message_category);
  CAF_CHECK_EQUAL(x.is_urgent_message(), false);
  CAF_CHECK_EQUAL(x.is_normal_message(), true);
  CAF_CHECK_EQUAL(x.request_id().integer_value(), 42u);
}

CAF_TEST(request with high priority) {
  auto x = make_message_id(42).response_id();
  CAF_CHECK_EQUAL(x.is_async(), false);
  CAF_CHECK_EQUAL(x.is_request(), false);
  CAF_CHECK_EQUAL(x.is_response(), true);
  CAF_CHECK_EQUAL(x.is_answered(), false);
  CAF_CHECK(x.category() == message_id::normal_message_category);
  CAF_CHECK_EQUAL(x.is_urgent_message(), false);
  CAF_CHECK_EQUAL(x.is_normal_message(), true);
  CAF_CHECK_EQUAL(x.request_id().integer_value(), 42u);
}

CAF_TEST(with_category) {
  auto x = make_message_id();
  CAF_CHECK(x.category() == message_id::normal_message_category);
  for (auto category : {message_id::urgent_message_category,
                        message_id::normal_message_category}) {
    x = x.with_category(category);
    CAF_CHECK_EQUAL(x.category(), category);
    CAF_CHECK_EQUAL(x.is_request(), false);
    CAF_CHECK_EQUAL(x.is_response(), false);
    CAF_CHECK_EQUAL(x.is_answered(), false);
  }
}
