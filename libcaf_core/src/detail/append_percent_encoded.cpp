// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
