// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE error

#include "caf/error.hpp"

#include "core-test.hpp"

using namespace caf;

CAF_TEST(default constructed errors evaluate to false) {
  error err;
  CAF_CHECK(!err);
}

CAF_TEST(error code zero is not an error) {
  CAF_CHECK(!error(sec::none));
  CAF_CHECK(!make_error(sec::none));
  CAF_CHECK(!error{error_code<sec>(sec::none)});
}

CAF_TEST(error codes that are not zero are errors) {
  CAF_CHECK(error(sec::unexpected_message));
  CAF_CHECK(make_error(sec::unexpected_message));
  CAF_CHECK(error{error_code<sec>(sec::unexpected_message)});
}

CAF_TEST(errors convert enums to their integer value) {
  CAF_CHECK_EQUAL(make_error(sec::unexpected_message).code(), 1u);
  CAF_CHECK_EQUAL(error{error_code<sec>(sec::unexpected_message)}.code(), 1u);
}
