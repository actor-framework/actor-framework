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

#include "caf/detail/run_sub_unit_test.hpp"

#include <sstream>
#include <iostream>

#include "caf/config.hpp"

#include "caf/string_algorithms.hpp"

#ifdef CAF_MSVC
# include <windows.h>
#endif

namespace caf {
namespace detail {

#ifndef CAF_WINDOWS
std::thread run_sub_unit_test(actor rc,
                              const char* cpath,
                              int max_runtime,
                              const char* suite_name,
                              bool set_asio_option,
                              std::initializer_list<std::string> args) {
  using namespace std;
  string path = cpath;
  replace_all(path, "'", "\\'");
  ostringstream oss;
  // set path and default options for sub unit tests
  oss << "'" << path << "' "
      << "-n -s " << suite_name << " -r " << max_runtime << " --";
  for (auto& arg : args)
    oss << " " << arg;
  if (set_asio_option)
    oss << " --use-asio";
  oss << " 2>&1";
  string cmdstr = oss.str();
  return std::thread{ [cmdstr, rc] {
    // on FreeBSD, popen() hangs indefinitely in some cases
#   ifdef CAF_BSD
    system(cmdstr.c_str());
    anon_send(rc, "");
#   else
    string output;
    auto fp = popen(cmdstr.c_str(), "r");
    if (! fp) {
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
#   endif
  }};
}
#else
std::thread run_sub_unit_test(actor rc,
                              const char* cpath,
                              int max_runtime,
                              const char* suite_name,
                              bool set_asio_option,
                              std::initializer_list<std::string> args) {
  std::string path = cpath;
  replace_all(path, "'", "\\'");
  std::ostringstream oss;
  // set path and default options for sub unit tests
  oss << path
      << " -n -s " << suite_name << " -r " << max_runtime << " --";
  for (auto& arg : args)
    oss << " " << arg;
  if (set_asio_option)
    oss << " --use-asio";
  return std::thread([rc](std::string cmdstr) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    CreateProcess(nullptr, // no module name
      &cmdstr[0], // command line
      nullptr, // process handle not inheritable
      nullptr, // thread handle not inheritable
      false, // no handle inheritance
      0, // no creation flags
      nullptr, // use parent's environment
      nullptr, // use parent's directory
      &si,
      &pi);
    // be a good parent and wait for our little child
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    anon_send(rc, "--- process output on windows not implemented yet ---");
  }, oss.str());
}
#endif

} // namespace detail
} // namespace caf
