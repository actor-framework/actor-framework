// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_string

#include "caf/detail/parser/read_string.hpp"

#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/expected.hpp"
#include "caf/parser_state.hpp"
#include "caf/string_view.hpp"
#include "caf/variant.hpp"

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
  expected<std::string> operator()(string_view str) {
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

CAF_TEST_FIXTURE_SCOPE(read_string_tests, fixture)

CAF_TEST(empty string) {
  CAF_CHECK_EQUAL(p(R"("")"), ""s);
  CAF_CHECK_EQUAL(p(R"( "")"), ""s);
  CAF_CHECK_EQUAL(p(R"(  "")"), ""s);
  CAF_CHECK_EQUAL(p(R"("" )"), ""s);
  CAF_CHECK_EQUAL(p(R"(""  )"), ""s);
  CAF_CHECK_EQUAL(p(R"(  ""  )"), ""s);
  CAF_CHECK_EQUAL(p("\t \"\" \t\t\t "), ""s);
  CAF_CHECK_EQUAL(p(R"('')"), ""s);
  CAF_CHECK_EQUAL(p(R"( '')"), ""s);
  CAF_CHECK_EQUAL(p(R"(  '')"), ""s);
  CAF_CHECK_EQUAL(p(R"('' )"), ""s);
  CAF_CHECK_EQUAL(p(R"(''  )"), ""s);
  CAF_CHECK_EQUAL(p(R"(  ''  )"), ""s);
  CAF_CHECK_EQUAL(p("\t '' \t\t\t "), ""s);
}

CAF_TEST(nonempty quoted string) {
  CAF_CHECK_EQUAL(p(R"("abc")"), "abc"s);
  CAF_CHECK_EQUAL(p(R"("a b c")"), "a b c"s);
  CAF_CHECK_EQUAL(p(R"(   "abcdefABCDEF"   )"), "abcdefABCDEF"s);
  CAF_CHECK_EQUAL(p(R"('abc')"), "abc"s);
  CAF_CHECK_EQUAL(p(R"('a b c')"), "a b c"s);
  CAF_CHECK_EQUAL(p(R"(   'abcdefABCDEF'   )"), "abcdefABCDEF"s);
}

CAF_TEST(quoted string with escaped characters) {
  CAF_CHECK_EQUAL(p(R"("a\tb\tc")"), "a\tb\tc"s);
  CAF_CHECK_EQUAL(p(R"("a\nb\r\nc")"), "a\nb\r\nc"s);
  CAF_CHECK_EQUAL(p(R"("a\\b")"), "a\\b"s);
  CAF_CHECK_EQUAL(p("\"'hello' \\\"world\\\"\""), "'hello' \"world\""s);
  CAF_CHECK_EQUAL(p(R"('a\tb\tc')"), "a\tb\tc"s);
  CAF_CHECK_EQUAL(p(R"('a\nb\r\nc')"), "a\nb\r\nc"s);
  CAF_CHECK_EQUAL(p(R"('a\\b')"), "a\\b"s);
  CAF_CHECK_EQUAL(p(R"('\'hello\' "world"')"), "'hello' \"world\""s);
}

CAF_TEST(unquoted strings) {
  CAF_CHECK_EQUAL(p(R"(foo)"), "foo"s);
  CAF_CHECK_EQUAL(p(R"( foo )"), "foo"s);
  CAF_CHECK_EQUAL(p(R"( 123 )"), "123"s);
}

CAF_TEST(invalid strings) {
  CAF_CHECK_EQUAL(p(R"("abc)"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(p(R"('abc)"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(p("\"ab\nc\""), pec::unexpected_newline);
  CAF_CHECK_EQUAL(p("'ab\nc'"), pec::unexpected_newline);
  CAF_CHECK_EQUAL(p(R"("abc" def)"), pec::trailing_character);
  CAF_CHECK_EQUAL(p(R"('abc' def)"), pec::trailing_character);
  CAF_CHECK_EQUAL(p(R"( 123, )"), pec::trailing_character);
}

CAF_TEST_FIXTURE_SCOPE_END()
