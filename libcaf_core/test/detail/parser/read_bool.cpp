// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_bool

#include "caf/detail/parser/read_bool.hpp"

#include "caf/test/unit_test.hpp"

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

CAF_TEST_FIXTURE_SCOPE(read_bool_tests, fixture)

CAF_TEST(valid booleans) {
  CAF_CHECK_EQUAL(p("true"), res_t{true});
  CAF_CHECK_EQUAL(p("false"), res_t{false});
}

CAF_TEST(invalid booleans) {
  CAF_CHECK_EQUAL(p(""), res_t{pec::unexpected_eof});
  CAF_CHECK_EQUAL(p("t"), res_t{pec::unexpected_eof});
  CAF_CHECK_EQUAL(p("tr"), res_t{pec::unexpected_eof});
  CAF_CHECK_EQUAL(p("tru"), res_t{pec::unexpected_eof});
  CAF_CHECK_EQUAL(p(" true"), res_t{pec::unexpected_character});
  CAF_CHECK_EQUAL(p("f"), res_t{pec::unexpected_eof});
  CAF_CHECK_EQUAL(p("fa"), res_t{pec::unexpected_eof});
  CAF_CHECK_EQUAL(p("fal"), res_t{pec::unexpected_eof});
  CAF_CHECK_EQUAL(p("fals"), res_t{pec::unexpected_eof});
  CAF_CHECK_EQUAL(p(" false"), res_t{pec::unexpected_character});
  CAF_CHECK_EQUAL(p("tr\nue"), res_t{pec::unexpected_newline});
  CAF_CHECK_EQUAL(p("trues"), res_t{pec::trailing_character});
}

CAF_TEST_FIXTURE_SCOPE_END()
