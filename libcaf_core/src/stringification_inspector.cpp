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

#include "caf/detail/stringification_inspector.hpp"

namespace caf {
namespace detail {

void stringification_inspector::sep() {
  if (! result_.empty())
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

void stringification_inspector::consume(const char* cstr) {
  if (! cstr || *cstr == '\0') {
    result_ += "\"\"";
    return;
  }
  if (*cstr == '"') {
    // assume an already escaped string
    result_ += cstr;
    return;
  }
  result_ += '"';
  char c;
  for(;;) {
    switch (c = *cstr++) {
      default:
        result_ += c;
        break;
      case '\\':
        result_ += "\\\\";
        break;
      case '"':
        result_ += "\\\"";
        break;
      case '\0':
        goto end_of_string;
    }
  }
  end_of_string:
  result_ += '"';
}

void stringification_inspector::consume_hex(const uint8_t* xs, size_t n) {
  if (n == 0) {
    result_ += "00";
    return;
  }
  auto tbl = "0123456789ABCDEF";
  char buf[3] = {0, 0, 0};
  for (size_t i = 0; i < n; ++i) {
    auto c = xs[i];
    buf[0] = tbl[c >> 4];
    buf[1] = tbl[c & 0x0F];
    result_ += buf;
  }
}

} // namespace detail
} // namespace caf
