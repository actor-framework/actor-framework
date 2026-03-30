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
  /// Indicates that the class uses `aligned_alloc` and `aligned_free`.
  aligned_alloc_and_free,
};

/// Checks whether a class has a static `memory_interface` member constant.
template <class T>
concept has_memory_interface = requires(T) {
  { T::memory_interface } -> std::convertible_to<memory_interface>;
};

template <class T>
struct memory_interface_oracle {
  static constexpr auto value = memory_interface::new_and_delete;
};

template <has_memory_interface T>
struct memory_interface_oracle<T> {
  static constexpr auto value = T::memory_interface;
};

/// Evaluates to `true` if `T` uses the memory interface `new_and_delete`.
template <class T>
inline constexpr bool uses_new_and_delete = memory_interface_oracle<T>::value
                                            == memory_interface::new_and_delete;

/// Evaluates to `true` if `T` uses the memory interface `malloc_and_free`.
template <class T>
inline constexpr bool uses_malloc_and_free
  = memory_interface_oracle<T>::value == memory_interface::malloc_and_free;

/// Evaluates to `true` if `T` uses the memory interface
/// `aligned_alloc_and_free`.
template <class T>
inline constexpr bool uses_aligned_alloc_and_free
  = memory_interface_oracle<T>::value
    == memory_interface::aligned_alloc_and_free;

} // namespace caf::detail
