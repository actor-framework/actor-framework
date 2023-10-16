// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/string.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"

#include "caf/flow/observable.hpp"

#include <string>
#include <string_view>

using namespace caf;
using namespace std::literals;

auto tostr(const std::vector<char>& chars) {
  return std::string{chars.begin(), chars.end()};
}

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("normalize_newlines converts all newline styles to UNIX style") {
  GIVEN("a string with mixed newline styles") {
    auto str = "foo\r\nbar\nbaz\rqux"sv;
    WHEN("applying normalize_newlines") {
      auto obs = make_observable() //
                   .from_container(str)
                   .transform(caf::flow::string::normalize_newlines());
      THEN("the result contains only UNIX style newlines") {
        auto chars = collect(std::move(obs));
        if (check(chars.has_value())) {
          check_eq(tostr(*chars), "foo\nbar\nbaz\nqux");
        }
      }
    }
    WHEN("applying normalize_newlines and only requesting 5 output elements") {
      auto obs = make_observable() //
                   .from_container(str)
                   .transform(caf::flow::string::normalize_newlines())
                   .take(5);
      THEN("the result is truncated and newlines count as a single element") {
        auto chars = collect(std::move(obs));
        if (check(chars.has_value())) {
          check_eq(tostr(*chars), "foo\nb");
        }
      }
    }
  }
  GIVEN("an input observable that produces an error") {
    WHEN("applying normalize_newlines") {
      auto obs = obs_error<char>() //
                   .transform(caf::flow::string::normalize_newlines());
      THEN("the result contains the error") {
        auto chars = collect(std::move(obs));
        if (check(!chars.has_value())) {
          check_eq(chars.error(), sec::runtime_error);
        }
      }
    }
  }
}

SCENARIO("to_lines splits a character sequence into lines") {
  GIVEN("a string with newlines") {
    auto str = "line1\nline2\nline3"sv;
    WHEN("applying to_lines") {
      auto obs = make_observable() //
                   .from_container(str)
                   .transform(caf::flow::string::to_lines());
      THEN("the result contains each line as a separate string") {
        auto lines = collect(std::move(obs));
        if (check(lines.has_value()) && check_eq(lines->size(), 3u)) {
          check_eq(lines->at(0), "line1");
          check_eq(lines->at(1), "line2");
          check_eq(lines->at(2), "line3");
        }
      }
    }
    WHEN("applying to_lines and only requesting 2 output elements") {
      auto obs = make_observable() //
                   .from_container(str)
                   .transform(caf::flow::string::to_lines())
                   .take(2);
      THEN("the result is truncated") {
        auto lines = collect(std::move(obs));
        if (check(lines.has_value()) && check_eq(lines->size(), 2u)) {
          check_eq(lines->at(0), "line1");
          check_eq(lines->at(1), "line2");
        }
      }
    }
  }
  GIVEN("a string a trailing newline") {
    auto str = "foo\n"sv;
    WHEN("applying to_lines") {
      auto obs = make_observable() //
                   .from_container(str)
                   .transform(caf::flow::string::to_lines());
      THEN("the result contains an empty string at the end") {
        auto lines = collect(std::move(obs));
        if (check(lines.has_value()) && check_eq(lines->size(), 2u)) {
          check_eq(lines->at(0), "foo");
          check_eq(lines->at(1), "");
        }
      }
    }
  }
  GIVEN("an input observable that produces an error") {
    WHEN("applying to_lines") {
      auto obs = obs_error<char>() //
                   .transform(caf::flow::string::to_lines());
      THEN("the result contains the error") {
        auto chars = collect(std::move(obs));
        if (check(!chars.has_value())) {
          check_eq(chars.error(), sec::runtime_error);
        }
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)

CAF_TEST_MAIN()
