// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/error.hpp"
#include "caf/is_error_code_enum.hpp"
#include "caf/raise_error.hpp"
#include "caf/unit.hpp"

#include <memory>
#include <new>
#include <ostream>
#include <type_traits>
#include <utility>

namespace caf {

namespace detail {

template <class F, class... Ts>
auto expected_from_fn(F&& f, Ts&&... xs) {
  using res_t = decltype(f(std::forward<Ts>(xs)...));
  if constexpr (std::is_void_v<res_t>) {
    f(std::forward<Ts>(xs)...);
    return expected<res_t>{};
  } else {
    return expected<res_t>{f(std::forward<Ts>(xs)...)};
  }
}

} // namespace detail

/// Represents the result of a computation which can either complete
/// successfully with an instance of type `T` or fail with an `error`.
/// @tparam T The type of the result.
template <class T>
class expected {
public:
  // -- member types -----------------------------------------------------------

  using value_type = T;

  using error_type = caf::error;

  template <class U>
  using rebind = expected<U>;

  // -- static member variables ------------------------------------------------

  /// Stores whether move construct and move assign never throw.
  static constexpr bool nothrow_move
    = std::is_nothrow_move_constructible<T>::value
      && std::is_nothrow_move_assignable<T>::value;

  /// Stores whether copy construct and copy assign never throw.
  static constexpr bool nothrow_copy
    = std::is_nothrow_copy_constructible<T>::value
      && std::is_nothrow_copy_assignable<T>::value;

  /// Stores whether swap() never throws.
  static constexpr bool nothrow_swap = std::is_nothrow_swappable_v<T>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class U, class = std::enable_if_t<std::is_convertible_v<U, T>
                                              || is_error_code_enum_v<U>>>
  expected(U x) {
    if constexpr (std::is_convertible_v<U, T>) {
      has_value_ = true;
      new (std::addressof(value_)) T(std::move(x));
    } else {
      has_value_ = false;
      new (std::addressof(error_)) caf::error(x);
    }
  }

  expected(T&& x) noexcept(nothrow_move) : has_value_(true) {
    new (std::addressof(value_)) T(std::move(x));
  }

  expected(const T& x) noexcept(nothrow_copy) : has_value_(true) {
    new (std::addressof(value_)) T(x);
  }

  expected(caf::error e) noexcept : has_value_(false) {
    new (std::addressof(error_)) caf::error{std::move(e)};
  }

  expected(const expected& other) noexcept(nothrow_copy) {
    construct(other);
  }

  expected(expected&& other) noexcept(nothrow_move) {
    construct(std::move(other));
  }

  template <class... Ts>
  expected(std::in_place_t, Ts&&... xs) : has_value_(true) {
    new (std::addressof(value_)) T(std::forward<Ts>(xs)...);
  }

  ~expected() {
    destroy();
  }

  expected& operator=(const expected& other) noexcept(nothrow_copy) {
    if (has_value() && other.has_value_)
      value_ = other.value_;
    else if (!has_value() && !other.has_value_)
      error_ = other.error_;
    else {
      destroy();
      construct(other);
    }
    return *this;
  }

  expected& operator=(expected&& other) noexcept(nothrow_move) {
    if (has_value() && other.has_value_)
      value_ = std::move(other.value_);
    else if (!has_value() && !other.has_value_)
      error_ = std::move(other.error_);
    else {
      destroy();
      construct(std::move(other));
    }
    return *this;
  }

  expected& operator=(const T& x) noexcept(nothrow_copy) {
    if (has_value()) {
      value_ = x;
    } else {
      destroy();
      has_value_ = true;
      new (std::addressof(value_)) T(x);
    }
    return *this;
  }

  expected& operator=(T&& x) noexcept(nothrow_move) {
    if (has_value()) {
      value_ = std::move(x);
    } else {
      destroy();
      has_value_ = true;
      new (std::addressof(value_)) T(std::move(x));
    }
    return *this;
  }

  template <class U>
  typename std::enable_if<std::is_convertible<U, T>::value, expected&>::type
  operator=(U x) {
    return *this = T{std::move(x)};
  }

  expected& operator=(caf::error e) noexcept {
    if (!has_value())
      error_ = std::move(e);
    else {
      destroy();
      has_value_ = false;
      new (std::addressof(error_)) caf::error(std::move(e));
    }
    return *this;
  }

  template <class Enum, class = std::enable_if_t<is_error_code_enum_v<Enum>>>
  expected& operator=(Enum code) {
    return *this = make_error(code);
  }

  // -- observers --------------------------------------------------------------

  /// @copydoc engaged
  explicit operator bool() const noexcept {
    return has_value_;
  }

  /// Returns `true` if the object holds a value (is engaged).
  bool has_value() const noexcept {
    return has_value_;
  }

  /// Returns `true` if the object holds a value (is engaged).
  [[deprecated("use has_value() instead")]] bool engaged() const noexcept {
    return has_value_;
  }

  // -- modifiers --------------------------------------------------------------

  template <class... Args>
  std::enable_if_t<std::is_nothrow_constructible_v<T, Args...>, T&>
  emplace(Args&&... args) noexcept {
    destroy();
    has_value_ = true;
    new (std::addressof(value_)) T(std::forward<Args>(args)...);
    return value_;
  }

  void swap(expected& other) noexcept(nothrow_move&& nothrow_swap) {
    expected tmp{std::move(other)};
    other = std::move(*this);
    *this = std::move(tmp);
  }

  // -- value access -----------------------------------------------------------

  /// Returns the contained value.
  /// @pre `has_value() == true`.
  [[deprecated("use value() instead")]] const T& cvalue() const noexcept {
    if (!has_value())
      CAF_RAISE_ERROR("bad_expected_access");
    return value_;
  }

  /// @copydoc cvalue
  T& value() & {
    if (!has_value())
      CAF_RAISE_ERROR("bad_expected_access");
    return value_;
  }

  /// @copydoc cvalue
  const T& value() const& {
    if (!has_value())
      CAF_RAISE_ERROR("bad_expected_access");
    return value_;
  }

  /// @copydoc cvalue
  T&& value() && {
    if (!has_value())
      CAF_RAISE_ERROR("bad_expected_access");
    return std::move(value_);
  }

  /// @copydoc cvalue
  const T&& value() const&& {
    if (!has_value())
      CAF_RAISE_ERROR("bad_expected_access");
    return std::move(value_);
  }

  /// Returns the contained value if there is one, otherwise returns `fallback`.
  template <class U>
  T value_or(U&& fallback) const& {
    if (has_value())
      return value_;
    else
      return T{std::forward<U>(fallback)};
  }

  /// Returns the contained value if there is one, otherwise returns `fallback`.
  template <class U>
  T value_or(U&& fallback) && {
    if (has_value())
      return std::move(value_);
    else
      return T{std::forward<U>(fallback)};
  }

  /// @copydoc cvalue
  T& operator*() & noexcept {
    return value_;
  }

  /// @copydoc cvalue
  const T& operator*() const& noexcept {
    return value_;
  }

  /// @copydoc cvalue
  T&& operator*() && noexcept {
    return std::move(value_);
  }

  /// @copydoc cvalue
  const T&& operator*() const&& noexcept {
    return std::move(value_);
  }

  /// @copydoc cvalue
  T* operator->() noexcept {
    return &value_;
  }

  /// @copydoc cvalue
  const T* operator->() const noexcept {
    return &value_;
  }

  // -- error access -----------------------------------------------------------

  /// Returns the contained error.
  /// @pre `has_value() == false`.
  [[deprecated("use error() instead")]] const caf::error&
  cerror() const noexcept {
    CAF_ASSERT(!has_value_);
    return error_;
  }

  /// @copydoc cerror
  caf::error& error() & noexcept {
    CAF_ASSERT(!has_value_);
    return error_;
  }

  /// @copydoc cerror
  const caf::error& error() const& noexcept {
    CAF_ASSERT(!has_value_);
    return error_;
  }

  /// @copydoc cerror
  caf::error&& error() && noexcept {
    CAF_ASSERT(!has_value_);
    return std::move(error_);
  }

  /// @copydoc cerror
  const caf::error&& error() const&& noexcept {
    CAF_ASSERT(!has_value_);
    return std::move(error_);
  }

  // -- monadic functions ------------------------------------------------------

  template <class F>
  auto and_then(F&& f) & {
    using res_t = decltype(f(value_));
    static_assert(detail::is_expected_v<res_t>, "F must return an expected");
    if (has_value())
      return f(value_);
    else
      return res_t{error_};
  }

  template <class F>
  auto and_then(F&& f) && {
    using res_t = decltype(f(std::move(value_)));
    static_assert(detail::is_expected_v<res_t>, "F must return an expected");
    if (has_value())
      return f(std::move(value_));
    else
      return res_t{std::move(error_)};
  }

  template <class F>
  auto and_then(F&& f) const& {
    using res_t = decltype(f(value_));
    static_assert(detail::is_expected_v<res_t>, "F must return an expected");
    if (has_value())
      return f(value_);
    else
      return res_t{error_};
  }

  template <class F>
  auto and_then(F&& f) const&& {
    using res_t = decltype(f(std::move(value_)));
    static_assert(detail::is_expected_v<res_t>, "F must return an expected");
    if (has_value())
      return f(std::move(value_));
    else
      return res_t{std::move(error_)};
  }

  template <class F>
  expected or_else(F&& f) & {
    using res_t = decltype(f(error_));
    if constexpr (std::is_void_v<res_t>) {
      if (!has_value())
        f(error_);
      return *this;
    } else {
      static_assert(std::is_same_v<expected, res_t>,
                    "F must return expected<T> or void");
      if (has_value())
        return expected{std::in_place, value_};
      else
        return f(error_);
    }
  }

  template <class F>
  expected or_else(F&& f) && {
    using res_t = decltype(f(std::move(error_)));
    if constexpr (std::is_void_v<res_t>) {
      if (!has_value())
        f(std::move(error_));
      return std::move(*this);
    } else {
      static_assert(std::is_same_v<expected, res_t>,
                    "F must return expected<T> or void");
      if (has_value())
        return expected{std::in_place, std::move(value_)};
      else
        return f(std::move(error_));
    }
  }

  template <class F>
  expected or_else(F&& f) const& {
    using res_t = decltype(f(error_));
    if constexpr (std::is_void_v<res_t>) {
      if (!has_value())
        f(error_);
      return *this;
    } else {
      static_assert(std::is_same_v<expected, res_t>,
                    "F must return expected<T> or void");
      if (has_value())
        return expected{std::in_place, value_};
      else
        return f(error_);
    }
  }

  template <class F>
  expected or_else(F&& f) const&& {
    using res_t = decltype(f(std::move(error_)));
    if constexpr (std::is_void_v<res_t>) {
      if (!has_value())
        f(std::move(error_));
      return std::move(*this);
    } else {
      static_assert(std::is_same_v<expected, res_t>,
                    "F must return expected<T> or void");
      if (has_value())
        return expected{std::in_place, std::move(value_)};
      else
        return f(std::move(error_));
    }
  }

  template <class F>
  auto transform(F&& f) & {
    using res_t = decltype(f(value_));
    static_assert(!detail::is_expected_v<res_t>,
                  "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn(std::forward<F>(f), value_);
    else
      return expected<res_t>{error_};
  }

  template <class F>
  auto transform(F&& f) && {
    using res_t = decltype(f(value_));
    static_assert(!detail::is_expected_v<res_t>,
                  "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn(std::forward<F>(f), std::move(value_));
    else
      return expected<res_t>{std::move(error_)};
  }

  template <class F>
  auto transform(F&& f) const& {
    using res_t = decltype(f(value_));
    static_assert(!detail::is_expected_v<res_t>,
                  "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn(std::forward<F>(f), value_);
    else
      return expected<res_t>{error_};
  }

  template <class F>
  auto transform(F&& f) const&& {
    using res_t = decltype(f(value_));
    static_assert(!detail::is_expected_v<res_t>,
                  "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn(std::forward<F>(f), std::move(value_));
    else
      return expected<res_t>{std::move(error_)};
  }

  template <class F>
  expected transform_or(F&& f) & {
    using res_t = decltype(f(error_));
    static_assert(std::is_same_v<caf::error, res_t>, "F must return an error");
    if (has_value())
      return {std::in_place, value_};
    else
      return expected{f(error_)};
  }

  template <class F>
  expected transform_or(F&& f) && {
    using res_t = decltype(f(error_));
    static_assert(std::is_same_v<caf::error, res_t>, "F must return an error");
    if (has_value())
      return {std::in_place, std::move(value_)};
    else
      return expected{f(std::move(error_))};
  }

  template <class F>
  expected transform_or(F&& f) const& {
    using res_t = decltype(f(error_));
    static_assert(std::is_same_v<caf::error, res_t>, "F must return an error");
    if (has_value())
      return {std::in_place, value_};
    else
      return expected{f(error_)};
  }

  template <class F>
  expected transform_or(F&& f) const&& {
    using res_t = decltype(f(error_));
    static_assert(std::is_same_v<caf::error, res_t>, "F must return an error");
    if (has_value())
      return {std::in_place, std::move(value_)};
    else
      return expected{f(std::move(error_))};
  }

private:
  void construct(expected&& other) noexcept(nothrow_move) {
    if (other.has_value_)
      new (std::addressof(value_)) T(std::move(other.value_));
    else
      new (std::addressof(error_)) caf::error(std::move(other.error_));
    has_value_ = other.has_value_;
  }

  void construct(const expected& other) noexcept(nothrow_copy) {
    if (other.has_value_)
      new (std::addressof(value_)) T(other.value_);
    else
      new (std::addressof(error_)) caf::error(other.error_);
    has_value_ = other.has_value_;
  }

  void destroy() {
    if (has_value())
      value_.~T();
    else
      error_.~error();
  }

  /// Denotes whether `value_` may be accessed. When false, error_ may be
  /// accessed.
  bool has_value_;

  union {
    /// Stores the contained value.
    T value_;

    /// Stores an error in case there is no value.
    caf::error error_;
  };
};

/// @relates expected
template <class T>
auto operator==(const expected<T>& x, const expected<T>& y)
  -> decltype(*x == *y) {
  return x && y ? *x == *y : (!x && !y ? x.error() == y.error() : false);
}

/// @relates expected
template <class T, class U>
auto operator==(const expected<T>& x, const U& y) -> decltype(*x == y) {
  return x ? *x == y : false;
}

/// @relates expected
template <class T, class U>
auto operator==(const T& x, const expected<U>& y) -> decltype(x == *y) {
  return y == x;
}

/// @relates expected
template <class T>
bool operator==(const expected<T>& x, const error& y) {
  return x ? false : x.error() == y;
}

/// @relates expected
template <class T>
bool operator==(const error& x, const expected<T>& y) {
  return y == x;
}

/// @relates expected
template <class T, class Enum>
std::enable_if_t<is_error_code_enum_v<Enum>, bool>
operator==(const expected<T>& x, Enum y) {
  return x == make_error(y);
}

/// @relates expected
template <class T, class Enum>
std::enable_if_t<is_error_code_enum_v<Enum>, bool>
operator==(Enum x, const expected<T>& y) {
  return y == make_error(x);
}

/// @relates expected
template <class T>
auto operator!=(const expected<T>& x, const expected<T>& y)
  -> decltype(*x == *y) {
  return !(x == y);
}

/// @relates expected
template <class T, class U>
auto operator!=(const expected<T>& x, const U& y) -> decltype(*x == y) {
  return !(x == y);
}

/// @relates expected
template <class T, class U>
auto operator!=(const T& x, const expected<U>& y) -> decltype(x == *y) {
  return !(x == y);
}

/// @relates expected
template <class T>
bool operator!=(const expected<T>& x, const error& y) {
  return !(x == y);
}

/// @relates expected
template <class T>
bool operator!=(const error& x, const expected<T>& y) {
  return !(x == y);
}

/// @relates expected
template <class T, class Enum>
std::enable_if_t<is_error_code_enum_v<Enum>, bool>
operator!=(const expected<T>& x, Enum y) {
  return !(x == y);
}

/// @relates expected
template <class T, class Enum>
std::enable_if_t<is_error_code_enum_v<Enum>, bool>
operator!=(Enum x, const expected<T>& y) {
  return !(x == y);
}

/// The pattern `expected<void>` shall be used for functions that may generate
/// an error but would otherwise return `bool`.
template <>
class expected<void> {
public:
  // -- member types -----------------------------------------------------------

  using value_type = void;

  using error_type = caf::error;

  template <class U>
  using rebind = expected<U>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class Enum, class = std::enable_if_t<is_error_code_enum_v<Enum>>>
  expected(Enum x) : error_(std::in_place, x) {
    // nop
  }

  expected() noexcept = default;

  [[deprecated("use the default constructor instead")]] //
  expected(unit_t) noexcept {
    // nop
  }

  expected(caf::error err) noexcept : error_(std::move(err)) {
    // nop
  }

  expected(const expected& other) = default;

  expected(expected&& other) noexcept = default;

  explicit expected(std::in_place_t) {
    // nop
  }

  expected& operator=(const expected& other) = default;

  expected& operator=(expected&& other) noexcept = default;

  expected& operator=(caf::error err) noexcept {
    error_ = std::move(err);
    return *this;
  }

  template <class Enum, class = std::enable_if_t<is_error_code_enum_v<Enum>>>
  expected& operator=(Enum code) {
    error_ = make_error(code);
    return *this;
  }

  // -- observers --------------------------------------------------------------

  /// Returns `true` if the object does *not* hold an error.
  explicit operator bool() const noexcept {
    return !error_;
  }

  /// Returns `true` if the object does *not* hold an error.
  bool has_value() const noexcept {
    return !error_;
  }

  // -- modifiers --------------------------------------------------------------

  template <class... Args>
  void emplace() noexcept {
    error_ = std::nullopt;
  }

  void swap(expected& other) noexcept {
    error_.swap(other.error_);
  }

  // -- value access -----------------------------------------------------------

  void value() const& {
    // nop
  }

  void value() && {
    // nop
  }

  void operator*() const noexcept {
    // nop
  }

  // -- error access -----------------------------------------------------------

  caf::error& error() & noexcept {
    CAF_ASSERT(!has_value());
    return *error_;
  }

  const caf::error& error() const& noexcept {
    CAF_ASSERT(!has_value());
    return *error_;
  }

  caf::error&& error() && noexcept {
    CAF_ASSERT(!has_value());
    return std::move(*error_);
  }

  const caf::error&& error() const&& noexcept {
    CAF_ASSERT(!has_value());
    return std::move(*error_);
  }

  // -- monadic functions ------------------------------------------------------

  template <class F>
  auto and_then(F&& f) & {
    using res_t = decltype(f());
    static_assert(detail::is_expected_v<res_t>, "F must return an expected");
    if (has_value())
      return f();
    else
      return res_t{*error_};
  }

  template <class F>
  auto and_then(F&& f) && {
    using res_t = decltype(f());
    static_assert(detail::is_expected_v<res_t>, "F must return an expected");
    if (has_value())
      return f();
    else
      return res_t{std::move(*error_)};
  }

  template <class F>
  auto and_then(F&& f) const& {
    using res_t = decltype(f());
    static_assert(detail::is_expected_v<res_t>, "F must return an expected");
    if (has_value())
      return f();
    else
      return res_t{*error_};
  }

  template <class F>
  auto and_then(F&& f) const&& {
    using res_t = decltype(f());
    static_assert(detail::is_expected_v<res_t>, "F must return an expected");
    if (has_value())
      return f();
    else
      return res_t{*error_};
  }

  template <class F>
  expected or_else(F&& f) & {
    using res_t = decltype(f(*error_));
    if constexpr (std::is_void_v<res_t>) {
      if (!has_value())
        f(*error_);
      return *this;
    } else {
      static_assert(std::is_same_v<expected, res_t>,
                    "F must return expected<T> or void");
      if (has_value())
        return expected{std::in_place};
      else
        return f(*error_);
    }
  }

  template <class F>
  expected or_else(F&& f) && {
    using res_t = decltype(f(std::move(*error_)));
    if constexpr (std::is_void_v<res_t>) {
      if (!has_value())
        f(std::move(*error_));
      return std::move(*this);
    } else {
      static_assert(std::is_same_v<expected, res_t>,
                    "F must return expected<T> or void");
      if (has_value())
        return expected{std::in_place};
      else
        return f(std::move(*error_));
    }
  }

  template <class F>
  expected or_else(F&& f) const& {
    using res_t = decltype(f(*error_));
    if constexpr (std::is_void_v<res_t>) {
      if (!has_value())
        f(*error_);
      return *this;
    } else {
      static_assert(std::is_same_v<expected, res_t>,
                    "F must return expected<T> or void");
      if (has_value())
        return expected{std::in_place};
      else
        return f(*error_);
    }
  }

  template <class F>
  expected or_else(F&& f) const&& {
    using res_t = decltype(f(std::move(*error_)));
    if constexpr (std::is_void_v<res_t>) {
      if (!has_value())
        f(std::move(*error_));
      return *this;
    } else {
      static_assert(std::is_same_v<expected, res_t>,
                    "F must return expected<T> or void");
      if (has_value())
        return expected{std::in_place};
      else
        return f(std::move(*error_));
    }
  }

  template <class F>
  auto transform(F&& f) & {
    using res_t = decltype(f());
    static_assert(!detail::is_expected_v<res_t>,
                  "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn(std::forward<F>(f));
    else
      return expected<res_t>{*error_};
  }

  template <class F>
  auto transform(F&& f) && {
    using res_t = decltype(f());
    static_assert(!detail::is_expected_v<res_t>,
                  "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn(std::forward<F>(f));
    else
      return expected<res_t>{std::move(*error_)};
  }

  template <class F>
  auto transform(F&& f) const& {
    using res_t = decltype(f());
    static_assert(!detail::is_expected_v<res_t>,
                  "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn(std::forward<F>(f));
    else
      return expected<res_t>{*error_};
  }

  template <class F>
  auto transform(F&& f) const&& {
    using res_t = decltype(f());
    static_assert(!detail::is_expected_v<res_t>,
                  "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn(std::forward<F>(f));
    else
      return expected<res_t>{*error_};
  }

  template <class F>
  expected transform_or(F&& f) & {
    using res_t = decltype(f(*error_));
    static_assert(std::is_same_v<caf::error, res_t>, "F must return an error");
    if (has_value())
      return expected{};
    else
      return expected{f(*error_)};
  }

  template <class F>
  expected transform_or(F&& f) && {
    using res_t = decltype(f(std::move(*error_)));
    static_assert(std::is_same_v<caf::error, res_t>, "F must return an error");
    if (has_value())
      return expected{};
    else
      return expected{f(std::move(*error_))};
  }

  template <class F>
  expected transform_or(F&& f) const& {
    using res_t = decltype(f(*error_));
    static_assert(std::is_same_v<caf::error, res_t>, "F must return an error");
    if (has_value())
      return expected{std::in_place};
    else
      return expected{f(*error_)};
  }

  template <class F>
  expected transform_or(F&& f) const&& {
    using res_t = decltype(f(std::move(*error_)));
    static_assert(std::is_same_v<caf::error, res_t>, "F must return an error");
    if (has_value())
      return expected{};
    else
      return expected{f(std::move(*error_))};
  }

private:
  std::optional<caf::error> error_;
};

/// @relates expected
inline bool operator==(const expected<void>& x, const expected<void>& y) {
  return (x && y) || (!x && !y && x.error() == y.error());
}

/// @relates expected
inline bool operator!=(const expected<void>& x, const expected<void>& y) {
  return !(x == y);
}

template <>
class [[deprecated("use expected<void> instead")]] expected<unit_t>
  : public expected<void> {
public:
  using expected<void>::expected;
};

template <class T>
std::string to_string(const expected<T>& x) {
  if (x)
    return deep_to_string(*x);
  return "!" + to_string(x.error());
}

inline std::string to_string(const expected<void>& x) {
  if (x)
    return "unit";
  return "!" + to_string(x.error());
}

} // namespace caf

namespace std {

template <class T>
[[deprecated("use caf::to_string instead")]] auto
operator<<(ostream& oss, const caf::expected<T>& x) -> decltype(oss << *x) {
  if (x)
    oss << *x;
  else
    oss << "!" << to_string(x.error());
  return oss;
}

} // namespace std
