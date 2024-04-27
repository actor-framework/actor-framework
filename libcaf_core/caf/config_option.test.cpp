// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/config_option.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/outline.hpp"
#include "caf/test/test.hpp"

#include "caf/config_value.hpp"
#include "caf/expected.hpp"
#include "caf/log/test.hpp"
#include "caf/make_config_option.hpp"

#include <sstream>

using namespace caf;
using namespace std::literals;

namespace {

config_option::meta_state
dummy_meta_state(std::string_view type_name = "dummy") {
  return {
    [](void*, config_value&) { return error{}; },
    [](const void*) { return config_value{}; },
    type_name,
  };
}

OUTLINE("config options parse their parameters for long, short and env names") {
  using std::string;
  auto dummy = dummy_meta_state();
  GIVEN("category <category>, name <name> and description <desc>") {
    auto [category, name, desc] = block_parameters<string, string, string>();
    WHEN("constructing a config option with these parameters") {
      THEN("long name is <long>, short name is <short>, env name is <env> "
           "and flat is <flat>") {
        auto [lname, sname, ename, flat]
          = block_parameters<string, string, string, bool>();
        auto uut = config_option{category, name, desc, &dummy};
        auto category_name = category;
        if (category_name.front() == '?')
          category_name.erase(category_name.begin());
        auto full_name = category_name + "." + lname;
        check_eq(category_name, uut.category());
        check_eq(lname, uut.long_name());
        check_eq(sname, uut.short_names());
        check_eq(ename, uut.env_var_name());
        check_eq(desc, uut.description());
        check_eq(full_name, uut.full_name());
        check_eq(flat, uut.has_flat_cli_name());
        check_eq(ename, uut.env_var_name_cstr());
        log::test::debug("copying config options must return equal objects");
        auto equal_to_uut = [&uut, this](const config_option& other,
                                         const detail::source_location& loc
                                         = detail::source_location::current()) {
          check_eq(uut.category(), other.category(), loc);
          check_eq(uut.long_name(), other.long_name(), loc);
          check_eq(uut.short_names(), other.short_names(), loc);
          check_eq(uut.env_var_name(), other.env_var_name(), loc);
          check_eq(uut.description(), other.description(), loc);
          check_eq(uut.full_name(), other.full_name(), loc);
          check_eq(uut.has_flat_cli_name(), other.has_flat_cli_name(), loc);
          check(strcmp(uut.env_var_name_cstr(), other.env_var_name_cstr()) == 0,
                loc);
        };
        log::test::debug("copy and move construct must return equal objects");
        auto cpy = uut;
        equal_to_uut(cpy);
        auto mv = std::move(cpy);
        equal_to_uut(mv);
        log::test::debug("copy and move assignment must return equal objects");
        auto cpy2 = config_option{"abc", "def", "ghi", &dummy};
        cpy2 = uut;
        equal_to_uut(cpy2);
        auto mv2 = config_option{"abc", "def", "ghi", &dummy};
        mv2 = std::move(cpy2);
        equal_to_uut(mv2);
      }
    }
  }
  EXAMPLES = R"_(
    | category | name          | desc | long | short | env        | flat  |
    | foo      | bar           | baz  | bar  |       | FOO_BAR    | false |
    | foo      | bar,b         | baz  | bar  | b     | FOO_BAR    | false |
    | foo      | bar,bB        | baz  | bar  | bB    | FOO_BAR    | false |
    | foo      | bar,,MY_VAR   | baz  | bar  |       | MY_VAR     | false |
    | foo      | bar,b,MY_VAR  | baz  | bar  | b     | MY_VAR     | false |
    | ?my-cat  | bar           | baz  | bar  |       | MY_CAT_BAR | true  |
    | ?my-cat  | bar,b         | baz  | bar  | b     | MY_CAT_BAR | true  |
    | ?my-cat  | bar,bB        | baz  | bar  | bB    | MY_CAT_BAR | true  |
    | ?my-cat  | bar,,MY_VAR   | baz  | bar  |       | MY_VAR     | true  |
    | ?my-cat  | bar,b,MY_VAR  | baz  | bar  | b     | MY_VAR     | true  |
  )_";
}

TEST("swapping two config options exchanges their values") {
  auto dummy1 = dummy_meta_state("dummy1");
  auto dummy2 = dummy_meta_state("dummy2");
  auto one = config_option{"cat1", "one", "option 1", &dummy1};
  auto two = config_option{"?cat2", "two", "option 2", &dummy2};
  check(!one.has_flat_cli_name());
  check_eq(one.category(), "cat1");
  check_eq(one.long_name(), "one");
  check_eq(one.type_name(), "dummy1");
  check(two.has_flat_cli_name());
  check_eq(two.category(), "cat2");
  check_eq(two.long_name(), "two");
  check_eq(two.type_name(), "dummy2");
  one.swap(two);
  check(one.has_flat_cli_name());
  check_eq(one.category(), "cat2");
  check_eq(one.long_name(), "two");
  check_eq(one.type_name(), "dummy2");
  check(!two.has_flat_cli_name());
  check_eq(two.category(), "cat1");
  check_eq(two.long_name(), "one");
  check_eq(two.type_name(), "dummy1");
}

TEST("boolean options are flags") {
  SECTION("config option without storage") {
    check(make_config_option<bool>("foo", "bar", "baz").is_flag());
    check(!make_config_option<int>("foo", "bar", "baz").is_flag());
  }
  SECTION("config option with storage") {
    auto tmp1 = false;
    check(make_config_option(tmp1, "foo", "bar", "baz").is_flag());
    auto tmp2 = 0;
    check(!make_config_option(tmp2, "foo", "bar", "baz").is_flag());
  }
}

TEST("config options with storage sync their value with the storage") {
  auto val = int32_t{0};
  auto uut = make_config_option(val, "foo", "bar", "baz");
  check_eq(uut.type_name(), "int32_t");
  SECTION("valid input") {
    auto input = config_value{"42"};
    check_eq(uut.sync(input), error{});
    check_eq(val, 42);
  }
  SECTION("invalid input") {
    auto input = config_value{"foo"};
    check_ne(uut.sync(input), error{});
    check_eq(val, 0);
  }
}

TEST("negated config options") {
  auto negated_value = false;
  auto true_option = config_value{true};
  const auto negated_option = make_negated_config_option(negated_value, "foo",
                                                         "bar", "baz");
  check(negated_option.is_flag());
  check_eq(negated_option.type_name(), "bool");
  check_eq(negated_option.category(), "foo");
  check_eq(negated_option.long_name(), "bar");
  check_eq(negated_option.short_names(), "");
  check_eq(negated_option.sync(true_option), error{});
}

} // namespace
