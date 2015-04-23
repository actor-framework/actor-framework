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

#include "caf/detail/run_program.hpp"

#include <iostream>

using namespace std;
using namespace caf;
using namespace caf::detail;

std::thread caf::detail::run_program_impl(actor rc, const char* cpath,
                                          vector<string> args) {
  string path = cpath;
  replace_all(path, "'", "\\'");
  ostringstream oss;
  oss << "'" << path << "'";
  for (auto& arg : args) {
    oss << " " << arg;
  }
  oss << " 2>&1";
  string cmdstr = oss.str();
  return thread([cmdstr, rc] {
    string output;
    auto fp = popen(cmdstr.c_str(), "r");
    if (!fp) {
      cerr << "FATAL: command line failed: " << cmdstr
           << endl;
      abort();
    }
    char buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
      output += buf;
    }
    pclose(fp);
    anon_send(rc, output);
  });
}
