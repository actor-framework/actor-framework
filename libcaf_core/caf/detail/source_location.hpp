// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>

namespace caf::detail {

// Backport of C++20's source_location. Remove when upgrading to C+20.
class source_location {
public:
  constexpr uint_least32_t line() const noexcept {
    return line_;
  }

  constexpr uint_least32_t column() const noexcept {
    return 0;
  }

  constexpr const char* file_name() const noexcept {
    return file_;
  }

  constexpr const char* function_name() const noexcept {
    return fn_;
  }

  static constexpr source_location
  current(const char* file_name = __builtin_FILE(),
          const char* fn_name = __builtin_FUNCTION(),
          int line = __builtin_LINE()) noexcept {
    source_location loc;
    loc.file_ = file_name;
    loc.fn_ = fn_name;
    loc.line_ = line;
    return loc;
  }

private:
  const char* file_ = "invalid";
  const char* fn_ = "invalid";
  uint_least32_t line_ = 0;
};

} // namespace caf::detail
