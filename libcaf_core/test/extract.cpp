/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE extract
#include "caf/test/unit_test.hpp"

#include <string>
#include <vector>

#include "caf/all.hpp"

namespace caf {

std::ostream& operator<<(std::ostream& out, const message& msg) {
  return out << to_string(msg);
}

} // namespace caf

using namespace caf;

using std::string;

// exclude this test; advanced match cases are currently not supported on MSVC
#ifndef CAF_WINDOWS
CAF_TEST(simple_ints) {
  auto msg = make_message(1, 2, 3);
  auto one = on(1) >> [] { };
  auto two = on(2) >> [] { };
  auto three = on(3) >> [] { };
  auto skip_two = [](int i) -> maybe<skip_message_t> {
    if (i == 2) {
      return skip_message();
    }
    return none;
  };
  CAF_CHECK_EQUAL(msg.extract(one), make_message(2, 3));
  CAF_CHECK_EQUAL(msg.extract(two), make_message(1, 3));
  CAF_CHECK_EQUAL(msg.extract(three), make_message(1, 2));
  CAF_CHECK_EQUAL(msg.extract(skip_two), make_message(2));
}
#endif // CAF_WINDOWS

CAF_TEST(type_sequences) {
  auto _64 = uint64_t{64};
  auto msg = make_message(1.0, 2.f, "str", 42, _64);
  auto df = [](double, float) { };
  auto fs = [](float, const string&) { };
  auto iu = [](int, uint64_t) { };
  CAF_CHECK_EQUAL(msg.extract(df), make_message("str", 42,  _64));
  CAF_CHECK_EQUAL(msg.extract(fs), make_message(1.0, 42,  _64));
  CAF_CHECK_EQUAL(msg.extract(iu), make_message(1.0, 2.f, "str"));
}

CAF_TEST(cli_args) {
  std::vector<string> args{"-n", "-v", "5", "--out-file=/dev/null"};
  int verbosity = 0;
  string output_file;
  string input_file;
  auto res = message_builder(args.begin(), args.end()).extract_opts({
    {"no-colors,n", "disable colors"},
    {"out-file,o", "redirect output", output_file},
    {"in-file,i", "read from file", input_file},
    {"verbosity,v", "1-5", verbosity}
  });
  CAF_CHECK_EQUAL(res.remainder.size(), 0);
  CAF_CHECK_EQUAL(to_string(res.remainder), to_string(message{}));
  CAF_CHECK_EQUAL(res.opts.count("no-colors"), 1);
  CAF_CHECK_EQUAL(res.opts.count("verbosity"), 1);
  CAF_CHECK_EQUAL(res.opts.count("out-file"), 1);
  CAF_CHECK_EQUAL(res.opts.count("in-file"), 0);
  CAF_CHECK_EQUAL(output_file, "/dev/null");
  CAF_CHECK_EQUAL(input_file, "");
}
