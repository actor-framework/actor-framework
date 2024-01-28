// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/error.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

using namespace caf;
using namespace std::literals;

namespace {

TEST("default-constructed errors evaluate to false") {
  error err;
  check(!err);
}

TEST("error code zero is not an error") {
  check(!error(sec::none));
  check(!make_error(sec::none));
  check(!error{error_code<sec>(sec::none)});
}

TEST("error codes that are not zero are errors") {
  check(static_cast<bool>(error(sec::unexpected_message)));
  check(static_cast<bool>(make_error(sec::unexpected_message)));
  check(static_cast<bool>(error{error_code<sec>(sec::unexpected_message)}));
}

TEST("errors convert enums to their integer value") {
  check_eq(make_error(sec::unexpected_message).code(), 1u);
  check_eq(error{error_code<sec>(sec::unexpected_message)}.code(), 1u);
}

TEST("make_error converts string-like arguments to strings") {
  using std::string;
  char foo1[] = "foo1";
  auto err = make_error(sec::runtime_error, foo1, "foo2"sv, "foo3", "foo4"s);
  if (check(static_cast<bool>(err))) {
    check_eq(err.code(), static_cast<uint8_t>(sec::runtime_error));
    check((err.context().match_elements<string, string, string, string>()));
  }
}

SCENARIO("errors provide human-readable to_string output") {
  auto err_str = [](auto... xs) {
    return to_string(make_error(std::move(xs)...));
  };
  GIVEN("an error object") {
    WHEN("converting an error without context to a string") {
      THEN("the output is only the error code") {
        check_eq(err_str(sec::invalid_argument), "invalid_argument");
      }
    }
    WHEN("converting an error with a context containing one element") {
      THEN("the output is the error code plus the context") {
        check_eq(err_str(sec::invalid_argument, "foo is not bar"),
                 R"_(invalid_argument("foo is not bar"))_");
      }
    }
    WHEN("converting an error with a context containing two or more elements") {
      THEN("the output is the error code plus all elements in the context") {
        check_eq(err_str(sec::invalid_argument, "want foo", "got bar"),
                 R"_(invalid_argument("want foo", "got bar"))_");
      }
    }
  }
}

} // namespace
