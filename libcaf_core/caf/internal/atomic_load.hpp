// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <type_traits>

#if defined(CAF_MSVC)
#  include <intrin.h>

#  include <cstring>
#endif

namespace caf::internal {

/// Atomically loads an integer or pointer with at least relaxed ordering.
template <class T>
T atomic_load_relaxed(const T& what) {
  static_assert(std::is_integral_v<T> || std::is_pointer_v<T>);
#if defined(CAF_MSVC)
  auto do_load = [&what] {
    if constexpr (sizeof(T) == 1) {
      auto* ptr = reinterpret_cast<const volatile __int8*>(&what);
      return __iso_volatile_load8(ptr);
    } else if constexpr (sizeof(T) == 2) {
      auto* ptr = reinterpret_cast<const volatile __int16*>(&what);
      return __iso_volatile_load16(ptr);
    } else if constexpr (sizeof(T) == 4) {
      auto* ptr = reinterpret_cast<const volatile __int32*>(&what);
      return __iso_volatile_load32(ptr);
    } else {
      static_assert(sizeof(T) == 8);
      auto* ptr = reinterpret_cast<const volatile __int64*>(&what);
      return __iso_volatile_load64(ptr);
    }
  };
  // The intrinsics operate on integer types, so we need to copy the bits to the
  // result for this to work for pointers as well.
  T result;
  auto bits = do_load();
  std::memcpy(&result, &bits, sizeof(T));
  return result;
#elif defined(CAF_GCC) || defined(CAF_CLANG)
  return __atomic_load_n(&what, __ATOMIC_RELAXED);
#else
#  error "atomic_load_relaxed requires compiler atomic intrinsics"
#endif
}

} // namespace caf::internal
