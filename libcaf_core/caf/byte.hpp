// Deprecated include. The next CAF release won't include this header.

#include <cstddef>
#include <type_traits>

#pragma once

namespace caf {

template <class IntegerType,
          class = std::enable_if_t<std::is_integral<IntegerType>::value>>
constexpr IntegerType to_integer(std::byte x) noexcept {
  return static_cast<IntegerType>(x);
}

} // namespace caf
