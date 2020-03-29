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

#include "caf/detail/append_percent_encoded.hpp"

#include "caf/byte.hpp"
#include "caf/config.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/string_view.hpp"

namespace caf::detail {

void append_percent_encoded(std::string& str, string_view x, bool is_path) {
  for (auto ch : x)
    switch (ch) {
      case ':':
      case '/':
        if (is_path) {
          str += ch;
          break;
        }
        [[fallthrough]];
      case ' ':
      case '?':
      case '#':
      case '[':
      case ']':
      case '@':
      case '!':
      case '$':
      case '&':
      case '\'':
      case '"':
      case '(':
      case ')':
      case '*':
      case '+':
      case ',':
      case ';':
      case '=':
        str += '%';
        append_hex(str, ch);
        break;
      default:
        str += ch;
    }
}

} // namespace caf::detail
