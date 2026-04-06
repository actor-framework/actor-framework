// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <type_traits>

namespace caf::detail {

/// Like `std::formatter`, but without the parse method and the signature of
/// `format` is just `OutputIterator format(T, OutputIterator) const`.
template <class T>
struct simple_formatter;

/// Tags a value for `detail::format` to render it using a `simple_formatter`.
template <class T>
struct formatted {
  using value_type = T;

  explicit formatted(const T& value) noexcept : value_(&value) {
    // nop
  }

  explicit formatted(const T* value) noexcept : value_(value) {
    // nop
  }

  const T* value_;
};

template <class T>
formatted(const T& value) -> formatted<std::remove_pointer_t<T>>;

template <class>
struct is_formatted_wrapper {
  static constexpr bool value = false;
};

template <class T>
struct is_formatted_wrapper<formatted<T>> {
  static constexpr bool value = true;
};

} // namespace caf::detail
