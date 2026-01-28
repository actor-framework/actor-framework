// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/match_wildcard_pattern.hpp"

namespace caf::detail {

bool match_wildcard_pattern(std::string_view input, std::string_view pattern) {
  // Keep track of our current position in the input.
  auto pos = input.begin();
  auto end = input.end();
  // Helper function to get the next character from the pattern.
  auto ppos = pattern.begin();
  auto next = [&pattern, &ppos] {
    if (ppos == pattern.end()) {
      return '\0';
    }
    return *ppos++;
  };
  // Extra bookkeeping for '*' backtracking.
  auto star_input_pos = input.end();
  auto star_pattern_pos = pattern.end();
  // Advances from the current position and attempts to match the pattern.
  auto advance = [&] {
    auto c = next();
    while (c != '\0') {
      switch (c) {
        case '?': // match exactly one character
          if (pos == end) {
            return false;
          }
          ++pos;
          break;
        case '*': // match zero or more characters
          if (ppos == pattern.end()) {
            return true; // a trailing '*' matches everything
          }
          while (*ppos == '*') {
            // skip consecutive '*' characters
            if (++ppos == pattern.end()) {
              return true;
            }
          }
          // Store the current position for backtracking. Initially, we assume
          // '*' consumes zero characters. Each backtrack then makes '*' consume
          // one more character until we find a match or have exhausted all
          // possibilities.
          star_input_pos = pos;
          star_pattern_pos = ppos;
          break;
        default: // input character must match current pattern character
          if (pos == end || *pos != c) {
            return false;
          }
          ++pos;
          break;
      }
      c = next();
      continue;
    }
    // If we've reached the end of the input, we've matched the pattern.
    return pos == end;
  };
  for (;;) {
    if (advance()) {
      return true;
    }
    // See if we can backtrack.
    if (star_input_pos != end) {
      // Jump back and make * consume one more character.
      pos = ++star_input_pos;
      ppos = star_pattern_pos;
    } else {
      // No more backtracking possible, we've failed to match the pattern.
      return false;
    }
  }
}

} // namespace caf::detail
