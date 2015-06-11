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

#include <sstream>

#include "caf/string_algorithms.hpp"

using std::string;
using std::vector;

namespace caf {

void split(vector<string>& result, const string& str, const string& delims,
           bool keep_all) {
  size_t pos = 0;
  size_t prev = 0;
  while ((pos = str.find_first_of(delims, prev)) != string::npos) {
    if (pos > prev) {
      auto substr = str.substr(prev, pos - prev);
      if (! substr.empty() || keep_all) {
        result.push_back(std::move(substr));
      }
    }
    prev = pos + 1;
  }
  if (prev < str.size()) {
    result.push_back(str.substr(prev, string::npos));
  }
}

} // namespace caf
