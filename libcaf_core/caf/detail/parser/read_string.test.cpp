// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/parser/read_string.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include "caf/expected.hpp"
#include "caf/parser_state.hpp"

#include <string>
#include <string_view>

using namespace caf;
using namespace std::literals;

namespace {

struct string_parser_consumer {
  std::string x;
  inline void value(std::string y) {
    x = std::move(y);
  }
};

struct string_parser {
  expected<std::string> operator()(std::string_view str) {
    string_parser_consumer f;
    string_parser_state res{str.begin(), str.end()};
    detail::parser::read_string(res, f);
    if (res.code == pec::success)
      return f.x;
    return make_error(res.code, res.column, std::string{res.i, res.e});
  }
};

struct fixture {
  string_parser p;
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("empty string") {
  check_eq(p(R"("")"), ""s);
  check_eq(p(R"( "")"), ""s);
  check_eq(p(R"(  "")"), ""s);
  check_eq(p(R"("" )"), ""s);
  check_eq(p(R"(""  )"), ""s);
  check_eq(p(R"(  ""  )"), ""s);
  check_eq(p("\t \"\" \t\t\t "), ""s);
  check_eq(p(R"('')"), ""s);
  check_eq(p(R"( '')"), ""s);
  check_eq(p(R"(  '')"), ""s);
  check_eq(p(R"('' )"), ""s);
  check_eq(p(R"(''  )"), ""s);
  check_eq(p(R"(  ''  )"), ""s);
  check_eq(p("\t '' \t\t\t "), ""s);
}

TEST("nonempty quoted string") {
  check_eq(p(R"("abc")"), "abc"s);
  check_eq(p(R"("a b c")"), "a b c"s);
  check_eq(p(R"(   "abcdefABCDEF"   )"), "abcdefABCDEF"s);
  check_eq(p(R"('abc')"), "abc"s);
  check_eq(p(R"('a b c')"), "a b c"s);
  check_eq(p(R"(   'abcdefABCDEF'   )"), "abcdefABCDEF"s);
}

TEST("quoted string with escaped characters") {
  check_eq(p(R"("a\tb\tc")"), "a\tb\tc"s);
  check_eq(p(R"("a\nb\r\nc")"), "a\nb\r\nc"s);
  check_eq(p(R"("a\\b")"), "a\\b"s);
  check_eq(p("\"'hello' \\\"world\\\"\""), "'hello' \"world\""s);
  check_eq(p(R"('a\tb\tc')"), "a\tb\tc"s);
  check_eq(p(R"('a\nb\r\nc')"), "a\nb\r\nc"s);
  check_eq(p(R"('a\\b')"), "a\\b"s);
  check_eq(p(R"('\'hello\' "world"')"), "'hello' \"world\""s);
}

TEST("unquoted strings") {
  check_eq(p(R"(foo)"), "foo"s);
  check_eq(p(R"( foo )"), "foo"s);
  check_eq(p(R"( 123 )"), "123"s);
}

TEST("invalid strings") {
  check_eq(p(R"("abc)"), pec::unexpected_eof);
  check_eq(p(R"('abc)"), pec::unexpected_eof);
  check_eq(p("\"ab\nc\""), pec::unexpected_newline);
  check_eq(p("'ab\nc'"), pec::unexpected_newline);
  check_eq(p(R"("abc" def)"), pec::trailing_character);
  check_eq(p(R"('abc' def)"), pec::trailing_character);
  check_eq(p(R"( 123, )"), pec::trailing_character);
}

} // WITH_FIXTURE(fixture)

CAF_TEST_MAIN()
