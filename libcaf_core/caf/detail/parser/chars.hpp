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

#pragma once

#include <cstring>

#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"

namespace caf::detail::parser {

struct any_char_t {};

constexpr any_char_t any_char = any_char_t{};

constexpr bool in_whitelist(any_char_t, char) noexcept {
  return true;
}

constexpr bool in_whitelist(char whitelist, char ch) noexcept {
  return whitelist == ch;
}

inline bool in_whitelist(const char* whitelist, char ch) noexcept {
  // Note: using strchr breaks if `ch == '\0'`.
  for (char c = *whitelist++; c != '\0'; c = *whitelist++)
    if (c == ch)
      return true;
  return false;
}

inline bool in_whitelist(bool (*filter)(char), char ch) noexcept {
  return filter(ch);
}

CAF_CORE_EXPORT extern const char alphanumeric_chars[63];

CAF_CORE_EXPORT extern const char alphabetic_chars[53];

CAF_CORE_EXPORT extern const char hexadecimal_chars[23];

CAF_CORE_EXPORT extern const char decimal_chars[11];

CAF_CORE_EXPORT extern const char octal_chars[9];

CAF_CORE_EXPORT extern const char quote_marks[3];

} // namespace caf::detail::parser
