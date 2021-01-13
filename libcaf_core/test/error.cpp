// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE error

#include "caf/error.hpp"

#include "core-test.hpp"

using namespace caf;

CAF_TEST(default constructed errors evaluate to false) {
  error err;
  CHECK(!err);
}

CAF_TEST(error code zero is not an error) {
  CHECK(!error(sec::none));
  CHECK(!make_error(sec::none));
  CHECK(!error{error_code<sec>(sec::none)});
}

CAF_TEST(error codes that are not zero are errors) {
  CHECK(error(sec::unexpected_message));
  CHECK(make_error(sec::unexpected_message));
  CHECK(error{error_code<sec>(sec::unexpected_message)});
}

CAF_TEST(errors convert enums to their integer value) {
  CHECK_EQ(make_error(sec::unexpected_message).code(), 1u);
  CHECK_EQ(error{error_code<sec>(sec::unexpected_message)}.code(), 1u);
}

SCENARIO("errors provide human-readable to_string output") {
  auto err_str = [](auto... xs) {
    return to_string(make_error(std::move(xs)...));
  };
  GIVEN("an error object") {
    WHEN("converting an error without context to a string") {
      THEN("the output is only the error code") {
        CHECK_EQ(err_str(sec::invalid_argument), "caf::sec::invalid_argument");
      }
    }
    WHEN("converting an error with a context containing one element") {
      THEN("the output is the error code plus the context") {
        CHECK_EQ(err_str(sec::invalid_argument, "foo is not bar"),
                 R"_(caf::sec::invalid_argument("foo is not bar"))_");
      }
    }
    WHEN("converting an error with a context containing two or more elements") {
      THEN("the output is the error code plus all elements in the context") {
        CHECK_EQ(err_str(sec::invalid_argument, "want foo", "got bar"),
                 R"_(caf::sec::invalid_argument("want foo", "got bar"))_");
      }
    }
  }
}
