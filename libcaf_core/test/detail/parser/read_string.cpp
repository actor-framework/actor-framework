// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_string

#include "caf/detail/parser/read_string.hpp"

#include "core-test.hpp"

#include <string>
#include <string_view>

#include "caf/expected.hpp"
#include "caf/parser_state.hpp"

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

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(empty string) {
  CHECK_EQ(p(R"("")"), ""s);
  CHECK_EQ(p(R"( "")"), ""s);
  CHECK_EQ(p(R"(  "")"), ""s);
  CHECK_EQ(p(R"("" )"), ""s);
  CHECK_EQ(p(R"(""  )"), ""s);
  CHECK_EQ(p(R"(  ""  )"), ""s);
  CHECK_EQ(p("\t \"\" \t\t\t "), ""s);
  CHECK_EQ(p(R"('')"), ""s);
  CHECK_EQ(p(R"( '')"), ""s);
  CHECK_EQ(p(R"(  '')"), ""s);
  CHECK_EQ(p(R"('' )"), ""s);
  CHECK_EQ(p(R"(''  )"), ""s);
  CHECK_EQ(p(R"(  ''  )"), ""s);
  CHECK_EQ(p("\t '' \t\t\t "), ""s);
}

CAF_TEST(nonempty quoted string) {
  CHECK_EQ(p(R"("abc")"), "abc"s);
  CHECK_EQ(p(R"("a b c")"), "a b c"s);
  CHECK_EQ(p(R"(   "abcdefABCDEF"   )"), "abcdefABCDEF"s);
  CHECK_EQ(p(R"('abc')"), "abc"s);
  CHECK_EQ(p(R"('a b c')"), "a b c"s);
  CHECK_EQ(p(R"(   'abcdefABCDEF'   )"), "abcdefABCDEF"s);
}

CAF_TEST(quoted string with escaped characters) {
  CHECK_EQ(p(R"("a\tb\tc")"), "a\tb\tc"s);
  CHECK_EQ(p(R"("a\nb\r\nc")"), "a\nb\r\nc"s);
  CHECK_EQ(p(R"("a\\b")"), "a\\b"s);
  CHECK_EQ(p("\"'hello' \\\"world\\\"\""), "'hello' \"world\""s);
  CHECK_EQ(p(R"('a\tb\tc')"), "a\tb\tc"s);
  CHECK_EQ(p(R"('a\nb\r\nc')"), "a\nb\r\nc"s);
  CHECK_EQ(p(R"('a\\b')"), "a\\b"s);
  CHECK_EQ(p(R"('\'hello\' "world"')"), "'hello' \"world\""s);
}

CAF_TEST(unquoted strings) {
  CHECK_EQ(p(R"(foo)"), "foo"s);
  CHECK_EQ(p(R"( foo )"), "foo"s);
  CHECK_EQ(p(R"( 123 )"), "123"s);
}

CAF_TEST(invalid strings) {
  CHECK_EQ(p(R"("abc)"), pec::unexpected_eof);
  CHECK_EQ(p(R"('abc)"), pec::unexpected_eof);
  CHECK_EQ(p("\"ab\nc\""), pec::unexpected_newline);
  CHECK_EQ(p("'ab\nc'"), pec::unexpected_newline);
  CHECK_EQ(p(R"("abc" def)"), pec::trailing_character);
  CHECK_EQ(p(R"('abc' def)"), pec::trailing_character);
  CHECK_EQ(p(R"( 123, )"), pec::trailing_character);
}

END_FIXTURE_SCOPE()
