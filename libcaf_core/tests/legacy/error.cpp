// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE error

#include "caf/error.hpp"

#include "core-test.hpp"

using namespace caf;
using namespace std::literals;

TEST_CASE("default-constructed errors evaluate to false") {
  error err;
  CHECK(!err);
}

TEST_CASE("error code zero is not an error") {
  CHECK(!error(sec::none));
  CHECK(!make_error(sec::none));
  CHECK(!error{error_code<sec>(sec::none)});
}

TEST_CASE("error codes that are not zero are errors") {
  CHECK(error(sec::unexpected_message));
  CHECK(make_error(sec::unexpected_message));
  CHECK(error{error_code<sec>(sec::unexpected_message)});
}

TEST_CASE("errors convert enums to their integer value") {
  CHECK_EQ(make_error(sec::unexpected_message).code(), 1u);
  CHECK_EQ(error{error_code<sec>(sec::unexpected_message)}.code(), 1u);
}

TEST_CASE("make_error converts string-like arguments to strings") {
  using std::string;
  char foo1[] = "foo1";
  auto err = make_error(sec::runtime_error, foo1, "foo2"sv, "foo3", "foo4"s);
  if (CHECK(err)) {
    CHECK_EQ(err.code(), static_cast<uint8_t>(sec::runtime_error));
    CHECK((err.context().match_elements<string, string, string, string>()));
  }
}

SCENARIO("errors provide human-readable to_string output") {
  auto err_str = [](auto... xs) {
    return to_string(make_error(std::move(xs)...));
  };
  GIVEN("an error object") {
    WHEN("converting an error without context to a string") {
      THEN("the output is only the error code") {
        CHECK_EQ(err_str(sec::invalid_argument), "invalid_argument");
      }
    }
    WHEN("converting an error with a context containing one element") {
      THEN("the output is the error code plus the context") {
        CHECK_EQ(err_str(sec::invalid_argument, "foo is not bar"),
                 R"_(invalid_argument("foo is not bar"))_");
      }
    }
    WHEN("converting an error with a context containing two or more elements") {
      THEN("the output is the error code plus all elements in the context") {
        CHECK_EQ(err_str(sec::invalid_argument, "want foo", "got bar"),
                 R"_(invalid_argument("want foo", "got bar"))_");
      }
    }
  }
}
