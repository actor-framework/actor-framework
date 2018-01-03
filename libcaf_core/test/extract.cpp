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

#include "caf/config.hpp"

#define CAF_SUITE extract
#include "caf/test/unit_test.hpp"

#include <string>
#include <vector>

#include "caf/all.hpp"

using namespace caf;

using std::string;

CAF_TEST(type_sequences) {
  auto _64 = uint64_t{64};
  std::string str = "str";
  auto msg = make_message(1.0, 2.f, str, 42, _64);
  auto df = [](double, float) { };
  auto fs = [](float, const string&) { };
  auto iu = [](int, uint64_t) { };
  CAF_CHECK_EQUAL(to_string(msg.extract(df)),
                  to_string(make_message(str, 42,  _64)));
  CAF_CHECK_EQUAL(to_string(msg.extract(fs)),
                  to_string(make_message(1.0, 42,  _64)));
  CAF_CHECK_EQUAL(to_string(msg.extract(iu)),
                  to_string(make_message(1.0, 2.f, str)));
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
  CAF_CHECK_EQUAL(res.remainder.size(), 0u);
  CAF_CHECK(res.remainder.empty());
  CAF_CHECK_EQUAL(res.opts.count("no-colors"), 1u);
  CAF_CHECK_EQUAL(res.opts.count("verbosity"), 1u);
  CAF_CHECK_EQUAL(res.opts.count("out-file"), 1u);
  CAF_CHECK_EQUAL(res.opts.count("in-file"), 0u);
  CAF_CHECK_EQUAL(output_file, "/dev/null");
  CAF_CHECK_EQUAL(input_file, "");
}
