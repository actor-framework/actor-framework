// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <atomic>
#include <type_traits>

#if defined(CAF_MSVC)
#  include <intrin.h>

#  include <cstring>
#endif

namespace caf::detail {

/// Atomically loads an integer or pointer with acquire ordering.
template <class T>
T atomic_load_acquire(const T& what) {
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
  // ISO volatile intrinsics do not provide ordering on their own.
  std::atomic_thread_fence(std::memory_order_acquire);
  std::memcpy(&result, &bits, sizeof(T));
  return result;
#elif defined(CAF_GCC) || defined(CAF_CLANG)
  return __atomic_load_n(&what, __ATOMIC_ACQUIRE);
#else
#  error "atomic_load_acquire requires compiler atomic intrinsics"
#endif
}

/// Atomically stores an integer or pointer with release ordering.
template <class T>
void atomic_store_release(T& what, T value) {
  static_assert(std::is_integral_v<T> || std::is_pointer_v<T>);
#if defined(CAF_MSVC)
  // The intrinsics take integer types, so preserve the bits for pointers too.
  // Pair the ISO volatile store with a release fence for publication.
  std::atomic_thread_fence(std::memory_order_release);
  if constexpr (sizeof(T) == 1) {
    __int8 bits;
    std::memcpy(&bits, &value, sizeof(T));
    __iso_volatile_store8(reinterpret_cast<volatile __int8*>(&what), bits);
  } else if constexpr (sizeof(T) == 2) {
    __int16 bits;
    std::memcpy(&bits, &value, sizeof(T));
    __iso_volatile_store16(reinterpret_cast<volatile __int16*>(&what), bits);
  } else if constexpr (sizeof(T) == 4) {
    __int32 bits;
    std::memcpy(&bits, &value, sizeof(T));
    __iso_volatile_store32(reinterpret_cast<volatile __int32*>(&what), bits);
  } else {
    static_assert(sizeof(T) == 8);
    __int64 bits;
    std::memcpy(&bits, &value, sizeof(T));
    __iso_volatile_store64(reinterpret_cast<volatile __int64*>(&what), bits);
  }
#elif defined(CAF_GCC) || defined(CAF_CLANG)
  __atomic_store_n(&what, value, __ATOMIC_RELEASE);
#else
#  error "atomic_store_release requires compiler atomic intrinsics"
#endif
}

} // namespace caf::detail
