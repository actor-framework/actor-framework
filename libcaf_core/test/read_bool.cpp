/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE read_bool

#include <string>

#include "caf/test/unit_test.hpp"

#include "caf/variant.hpp"

#include "caf/detail/parser/read_bool.hpp"

using namespace caf;

using detail::parser::ec;

namespace {

struct bool_parser_consumer {
  bool x;
  inline void value(bool y) {
    x = y;
  }
};

using res_t = variant<ec, bool>;

struct bool_parser {
  res_t operator()(std::string str) {
    detail::parser::state<std::string::iterator> res;
    bool_parser_consumer f;
    res.i = str.begin();
    res.e = str.end();
    detail::parser::read_bool(res, f);
    if (res.code == ec::success)
      return f.x;
    return res.code;
  }
};

struct fixture {
  bool_parser p;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(read_bool_tests, fixture)

CAF_TEST(valid booleans) {
  CAF_CHECK_EQUAL(p("true"), true);
  CAF_CHECK_EQUAL(p(" true"), true);
  CAF_CHECK_EQUAL(p("  true"), true);
  CAF_CHECK_EQUAL(p("true "), true);
  CAF_CHECK_EQUAL(p("true  "), true);
  CAF_CHECK_EQUAL(p("  true  "), true);
  CAF_CHECK_EQUAL(p("\t true \t\t\t "), true);
  CAF_CHECK_EQUAL(p("false"), false);
  CAF_CHECK_EQUAL(p(" false"), false);
  CAF_CHECK_EQUAL(p("  false"), false);
  CAF_CHECK_EQUAL(p("false "), false);
  CAF_CHECK_EQUAL(p("false  "), false);
  CAF_CHECK_EQUAL(p("  false  "), false);
  CAF_CHECK_EQUAL(p("\t false \t\t\t "), false);
}

CAF_TEST(invalid booleans) {
  CAF_CHECK_EQUAL(p(""), ec::unexpected_eof);
  CAF_CHECK_EQUAL(p("t"), ec::unexpected_eof);
  CAF_CHECK_EQUAL(p("tr"), ec::unexpected_eof);
  CAF_CHECK_EQUAL(p("tru"), ec::unexpected_eof);
  CAF_CHECK_EQUAL(p("f"), ec::unexpected_eof);
  CAF_CHECK_EQUAL(p("fa"), ec::unexpected_eof);
  CAF_CHECK_EQUAL(p("fal"), ec::unexpected_eof);
  CAF_CHECK_EQUAL(p("fals"), ec::unexpected_eof);
  CAF_CHECK_EQUAL(p("tr\nue"), ec::unexpected_newline);
  CAF_CHECK_EQUAL(p("trues"), ec::trailing_character);
}

CAF_TEST_FIXTURE_SCOPE_END()
