// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

namespace caf::detail {

template <class To, bool LargeUnsigned = sizeof(To) >= sizeof(int64_t)
                                         && std::is_unsigned<To>::value>
struct bounds_checker {
  static constexpr bool check(int64_t x) noexcept {
    return x >= std::numeric_limits<To>::min()
           && x <= std::numeric_limits<To>::max();
  }
};

template <>
struct bounds_checker<int64_t, false> {
  static constexpr bool check(int64_t) noexcept {
    return true;
  }
};

template <class To>
struct bounds_checker<To, true> {
  static constexpr bool check(int64_t x) noexcept {
    return x >= 0;
  }
};

} // namespace caf::detail
