// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"

#include <cstring>

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
