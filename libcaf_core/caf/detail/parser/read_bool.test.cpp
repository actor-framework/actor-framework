// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/parser/read_bool.hpp"

#include "caf/test/test.hpp"

#include "caf/parser_state.hpp"

#include <string>
#include <string_view>
#include <variant>

using namespace caf;

namespace {

struct bool_parser_consumer {
  bool x;
  void value(bool y) {
    x = y;
  }
};

using res_t = std::variant<pec, bool>;

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

WITH_FIXTURE(fixture) {

TEST("valid booleans") {
  check_eq(p("true"), res_t{true});
  check_eq(p("false"), res_t{false});
}

TEST("invalid booleans") {
  check_eq(p(""), res_t{pec::unexpected_eof});
  check_eq(p("t"), res_t{pec::unexpected_eof});
  check_eq(p("tr"), res_t{pec::unexpected_eof});
  check_eq(p("tru"), res_t{pec::unexpected_eof});
  check_eq(p(" true"), res_t{pec::unexpected_character});
  check_eq(p("f"), res_t{pec::unexpected_eof});
  check_eq(p("fa"), res_t{pec::unexpected_eof});
  check_eq(p("fal"), res_t{pec::unexpected_eof});
  check_eq(p("fals"), res_t{pec::unexpected_eof});
  check_eq(p(" false"), res_t{pec::unexpected_character});
  check_eq(p("tr\nue"), res_t{pec::unexpected_newline});
  check_eq(p("trues"), res_t{pec::trailing_character});
}

} // WITH_FIXTURE(fixture)

} // namespace
