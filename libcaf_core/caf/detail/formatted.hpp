// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/policy/by_reference.hpp"
#include "caf/policy/by_value.hpp"

#include <type_traits>
#include <utility>

namespace caf::detail {

/// Like `std::formatter`, but without the parse method and the signature of
/// `format` is just `OutputIterator format(T, OutputIterator) const`.
template <class T>
struct simple_formatter;

/// Tags a value for `detail::format` to render it using a `simple_formatter`.
template <class T, class Policy>
class formatted;

template <class T>
class formatted<T, policy::by_reference_t> {
public:
  using value_type = T;

  explicit formatted(const T& value) noexcept : value_(&value) {
    // nop
  }

  explicit formatted(const T* value) noexcept : value_(value) {
    // nop
  }

  formatted(const T& value, policy::by_reference_t) noexcept
    : formatted(value) {
    // nop
  }

  formatted(const T* value, policy::by_reference_t) noexcept
    : formatted(value) {
    // nop
  }

  const T& value() const {
    return *value_;
  }

  bool has_value() const noexcept {
    return value_ != nullptr;
  }

private:
  const T* value_;
};

template <class T>
class formatted<T, policy::by_value_t> {
public:
  using value_type = T;

  formatted(T val, policy::by_value_t) noexcept(
    std::is_nothrow_move_constructible_v<T>)
    : value_(std::move(val)) {
    // nop
  }

  const T& value() const noexcept {
    return value_;
  }

  bool has_value() const noexcept {
    if constexpr (std::is_pointer_v<T>)
      return value_ != nullptr;
    return true;
  }

private:
  T value_;
};

template <class T>
formatted(const T& value)
  -> formatted<std::remove_cv_t<std::remove_pointer_t<std::decay_t<T>>>,
               policy::by_reference_t>;

template <class T>
formatted(T, policy::by_value_t)
  -> formatted<std::decay_t<T>, policy::by_value_t>;

template <class>
struct is_formatted_wrapper {
  static constexpr bool value = false;
};

template <class T, class Policy>
struct is_formatted_wrapper<formatted<T, Policy>> {
  static constexpr bool value = true;
};

} // namespace caf::detail
