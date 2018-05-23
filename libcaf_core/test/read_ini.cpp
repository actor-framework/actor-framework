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

#include <string>
#include <vector>

#include "caf/config.hpp"

#define CAF_SUITE read_ini
#include "caf/test/dsl.hpp"

#include "caf/detail/parser/read_ini.hpp"

using namespace caf;

namespace {

using log_type = std::vector<std::string>;

struct test_consumer {
  log_type log;

  void begin_section(std::string name) {
    name.insert(0, "begin section: ");
    log.emplace_back(std::move(name));
  }

  void end_section() {
    log.emplace_back("end section");
  }
};

struct fixture {
  expected<log_type> parse(std::string str, bool expect_success = true) {
    detail::parser::state<std::string::iterator> res;
    test_consumer f;
    res.i = str.begin();
    res.e = str.end();
    detail::parser::read_ini(res, f);
    if (res.code == detail::parser::ec::success != expect_success)
      CAF_FAIL("unexpected parser result state");
    return std::move(f.log);
  }
};

template <class... Ts>
log_type make_log(Ts&&... xs) {
  return log_type{std::forward<Ts>(xs)...};
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(read_ini_tests, fixture)

CAF_TEST(empty inits) {
  CAF_CHECK_EQUAL(parse(";foo"), make_log());
  CAF_CHECK_EQUAL(parse("[foo]"), make_log("begin section: foo", "end section"));
}

CAF_TEST_FIXTURE_SCOPE_END()
