// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <concepts>

namespace caf::detail {

/// Specifies which allocation and deallocation functions a class uses.
enum class memory_interface {
  /// Indicates that the class uses `new` and `delete`. This is the default.
  new_and_delete,
  /// Indicates that the class uses `malloc` and `free`.
  malloc_and_free,
  /// Indicates that the class uses `malloc` and `free`.
  aligned_alloc_and_free,
};

/// Checks whether a class has a static `memory_interface` member constant.
template <class T>
concept has_memory_interface = requires(T) {
  { T::memory_interface } -> std::convertible_to<memory_interface>;
};

/// Checks whether `T` uses the memory interface `new_and_delete`.
template <class T>
constexpr bool uses_new_and_delete() noexcept {
  if constexpr (has_memory_interface<T>) {
    return T::memory_interface == memory_interface::new_and_delete;
  } else {
    return true;
  }
}

/// Checks whether `T` uses the memory interface `malloc_and_free`.
template <class T>
constexpr bool uses_malloc_and_free() noexcept {
  if constexpr (has_memory_interface<T>) {
    return T::memory_interface == memory_interface::malloc_and_free;
  } else {
    return false;
  }
}

/// Checks whether `T` uses the memory interface `aligned_alloc_and_free`.
template <class T>
constexpr bool uses_aligned_alloc_and_free() noexcept {
  if constexpr (has_memory_interface<T>) {
    return T::memory_interface == memory_interface::aligned_alloc_and_free;
  } else {
    return false;
  }
}

} // namespace caf::detail
