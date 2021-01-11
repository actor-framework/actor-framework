// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE json_writer

#include "caf/json_writer.hpp"

#include "core-test.hpp"

using namespace caf;

using namespace std::literals::string_literals;

namespace {

struct fixture {
  template <class T>
  expected<std::string> to_json_string(T&& x, size_t indentation_factor) {
    json_writer writer;
    writer.indentation(indentation_factor);
    if (writer.apply(std::forward<T>(x))) {
      auto buf = writer.str();
      return {std::string{buf.begin(), buf.end()}};
    } else {
      MESSAGE("partial JSON output: " << writer.str());
      return {writer.get_error()};
    }
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(json_writer_tests, fixture)

SCENARIO("the JSON writer converts builtin types to strings") {
  GIVEN("an integer") {
    auto x = 42;
    WHEN("converting it to JSON with any indentation factor") {
      THEN("the JSON output is the number") {
        CHECK_EQ(to_json_string(x, 0), "42"s);
        CHECK_EQ(to_json_string(x, 2), "42"s);
      }
    }
  }
  GIVEN("a string") {
    auto x = R"_(hello "world"!)_"s;
    WHEN("converting it to JSON with any indentation factor") {
      THEN("the JSON output is the escaped string") {
        CHECK_EQ(to_json_string(x, 0), R"_("hello \"world\"!")_"s);
        CHECK_EQ(to_json_string(x, 2), R"_("hello \"world\"!")_"s);
      }
    }
  }
  GIVEN("a list") {
    auto x = std::vector<int>{1, 2, 3};
    WHEN("converting it to JSON with indentation factor 0") {
      THEN("the JSON output is a single line") {
        CHECK_EQ(to_json_string(x, 0), "[1, 2, 3]"s);
      }
    }
    WHEN("converting it to JSON with indentation factor 2") {
      THEN("the JSON output uses multiple lines") {
        auto out = R"_([
  1,
  2,
  3
])_"s;
        CHECK_EQ(to_json_string(x, 2), out);
      }
    }
  }
  GIVEN("a dictionary") {
    std::map<std::string, std::string> x;
    x.emplace("a", "A");
    x.emplace("b", "B");
    x.emplace("c", "C");
    WHEN("converting it to JSON with indentation factor 0") {
      THEN("the JSON output is a single line") {
        CHECK_EQ(to_json_string(x, 0), R"_({"a": "A", "b": "B", "c": "C"})_"s);
      }
    }
    WHEN("converting it to JSON with indentation factor 2") {
      THEN("the JSON output uses multiple lines") {
        auto out = R"_({
  "a": "A",
  "b": "B",
  "c": "C"
})_"s;
        CHECK_EQ(to_json_string(x, 2), out);
      }
    }
  }
  GIVEN("a message") {
    auto x = make_message(put_atom_v, "foo", 42);
    WHEN("converting it to JSON with indentation factor 0") {
      THEN("the JSON output is a single line") {
        CHECK_EQ(to_json_string(x, 0),
                 R"_([{"@type": "caf::put_atom"}, "foo", 42])_"s);
      }
    }
    WHEN("converting it to JSON with indentation factor 2") {
      THEN("the JSON output uses multiple lines") {
        auto out = R"_([
  {
    "@type": "caf::put_atom"
  },
  "foo",
  42
])_"s;
        CHECK_EQ(to_json_string(x, 2), out);
      }
    }
  }
}

SCENARIO("the JSON writer converts simple structs to strings") {
  GIVEN("a dummy_struct object") {
    dummy_struct x{10, "foo"};
    WHEN("converting it to JSON with indentation factor 0") {
      THEN("the JSON output is a single line") {
        auto out = R"_({"@type": "dummy_struct", "a": 10, "b": "foo"})_"s;
        CHECK_EQ(to_json_string(x, 0), out);
      }
    }
    WHEN("converting it to JSON with indentation factor 2") {
      THEN("the JSON output uses multiple lines") {
        auto out = R"_({
  "@type": "dummy_struct",
  "a": 10,
  "b": "foo"
})_"s;
        CHECK_EQ(to_json_string(x, 2), out);
      }
    }
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
