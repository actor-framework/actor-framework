// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_bool

#include "caf/detail/parser/read_bool.hpp"

#include "core-test.hpp"

#include <string>
#include <string_view>

#include "caf/parser_state.hpp"
#include "caf/variant.hpp"

using namespace caf;

namespace {

struct bool_parser_consumer {
  bool x;
  inline void value(bool y) {
    x = y;
  }
};

using res_t = variant<pec, bool>;

struct bool_parser {
  res_t operator()(std::string_view str) {
    bool_parser_consumer f;
    string_parser_state res{str.begin(), str.end()};
    detail::parser::read_bool(res, f);
    if (res.code == pec::success)
      return f.x;
    else
      return res.code;
  }
};

struct fixture {
  bool_parser p;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(valid booleans) {
  CHECK_EQ(p("true"), res_t{true});
  CHECK_EQ(p("false"), res_t{false});
}

CAF_TEST(invalid booleans) {
  CHECK_EQ(p(""), res_t{pec::unexpected_eof});
  CHECK_EQ(p("t"), res_t{pec::unexpected_eof});
  CHECK_EQ(p("tr"), res_t{pec::unexpected_eof});
  CHECK_EQ(p("tru"), res_t{pec::unexpected_eof});
  CHECK_EQ(p(" true"), res_t{pec::unexpected_character});
  CHECK_EQ(p("f"), res_t{pec::unexpected_eof});
  CHECK_EQ(p("fa"), res_t{pec::unexpected_eof});
  CHECK_EQ(p("fal"), res_t{pec::unexpected_eof});
  CHECK_EQ(p("fals"), res_t{pec::unexpected_eof});
  CHECK_EQ(p(" false"), res_t{pec::unexpected_character});
  CHECK_EQ(p("tr\nue"), res_t{pec::unexpected_newline});
  CHECK_EQ(p("trues"), res_t{pec::trailing_character});
}

END_FIXTURE_SCOPE()
