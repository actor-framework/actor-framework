/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include <vector>
#include <string>
#include <sstream>

#include "caf/match.hpp"

using namespace std;

namespace caf {

detail::match_helper match_split(const std::string& str, char delim,
                                 bool keep_empties) {
  message_builder result;
  stringstream strs(str);
  string tmp;
  while (getline(strs, tmp, delim)) {
    if (!tmp.empty() && !keep_empties) {
      result.append(std::move(tmp));
    }
  }
  return result.to_message();
}

} // namespace caf
