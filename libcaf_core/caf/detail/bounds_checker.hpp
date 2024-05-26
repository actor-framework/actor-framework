// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

namespace caf::detail {

template <class To>
struct bounds_checker {
  template <class From>
  static constexpr bool check(From x) noexcept {
    auto converted = static_cast<To>(x);
    if constexpr (std::is_signed_v<From> == std::is_signed_v<To>) {
      // If the source and target types have the same signedness, we can simply
      // check whether the value is in the range of the target type.
      return static_cast<From>(converted) == x;
    } else if constexpr (std::is_signed_v<From>) {
      // If the source type is signed and the target type is unsigned, we need
      // to check that the value is not negative. Otherwise, the conversion
      // could yield a positive value, which is out of range for the target
      // type.
      return x >= 0 && static_cast<From>(converted) == x;
    } else {
      static_assert(std::is_signed_v<To>);
      // If the source type is unsigned and the target type is signed, we need
      // to check whether the conversion produced a non-negative value. If the
      // value is negative, the conversion is out of range.
      return converted >= 0 && static_cast<From>(converted) == x;
    }
  }
};

} // namespace caf::detail
