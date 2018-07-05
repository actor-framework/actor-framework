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

#include "caf/detail/stringification_inspector.hpp"

namespace caf {
namespace detail {

void stringification_inspector::sep() {
  if (!result_.empty())
    switch (result_.back()) {
      case '(':
      case '[':
      case ' ': // only at back if we've printed ", " before
        break;
      default:
        result_ += ", ";
    }
}

void stringification_inspector::consume(atom_value& x) {
  result_ += '\'';
  result_ += to_string(x);
  result_ += '\'';
}

void stringification_inspector::consume(string_view str) {
  if (str.empty()) {
    result_ += R"("")";
    return;
  }
  if (str[0] == '"') {
    // Assume an already escaped string.
    result_.insert(result_.end(), str.begin(), str.end());
    return;
  }
  // Escape string.
  result_ += '"';
  for (char c : str) {
    switch (c) {
      default:
        result_ += c;
        break;
      case '\\':
        result_ += R"(\\)";
        break;
      case '"':
        result_ += R"(\")";
        break;
    }
  }
  result_ += '"';
}

} // namespace detail
} // namespace caf
