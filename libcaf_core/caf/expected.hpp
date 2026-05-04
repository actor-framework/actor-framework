// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

// NOLINTBEGIN(bugprone-unchecked-optional-access)

#include "caf/detail/assert.hpp"
#include "caf/detail/build_config.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/error.hpp"
#include "caf/error_code_enum.hpp"
#include "caf/raise_error.hpp"

#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace caf::detail {

/// Invokes `f` and wraps the result in `expected` with unexpected type `E`.
template <class E = error, class F, class... Ts>
auto expected_from_fn(F&& f, Ts&&... xs) {
  using res_t = decltype(f(std::forward<Ts>(xs)...));
  if constexpr (std::is_void_v<res_t>) {
    f(std::forward<Ts>(xs)...);
    return expected<void, E>{};
  } else {
    return expected<res_t, E>{f(std::forward<Ts>(xs)...)};
  }
}

} // namespace caf::detail

#ifdef CAF_USE_STD_EXPECTED

#  include <expected>

namespace caf {

using unexpect_t = std::unexpect_t;

inline constexpr std::unexpect_t unexpect{};

} // namespace caf

#else

namespace caf {

struct unexpect_t {};

inline constexpr unexpect_t unexpect{};

/// Represents an unexpected value to be stored in caf::expected.
template <class E>
class unexpected {
public:
  template <typename... Args>
    requires std::is_constructible_v<E, Args...>
  constexpr explicit unexpected(std::in_place_t, Args&&... args)
    : error_{std::forward<Args>(args)...} {
    // nop
  }

  template <class U, class... Args>
    requires std::is_constructible_v<E, std::initializer_list<U>&, Args...>
  constexpr explicit unexpected(std::in_place_t, std::initializer_list<U> il,
                                Args&&... args)
    : error_{std::move(il), std::forward<Args>(args)...} {
    // nop
  }

  explicit unexpected(E err) : error_{std::move(err)} {
    // nop
  }

  E& error() & noexcept {
    return error_;
  }

  E&& error() && noexcept {
    return std::move(error_);
  }

  const E& error() const& noexcept {
    return error_;
  }

  const E&& error() const&& noexcept {
    return std::move(error_);
  }

  void swap(unexpected& other) noexcept {
    using std::swap;
    swap(error_, other.error_);
  }

private:
  E error_;
};

/// @relates unexpected
template <class E>
unexpected(E) -> unexpected<E>;

/// @relates unexpected
template <class E1, class E2>
inline bool
operator==(const unexpected<E1>& lhs, const unexpected<E2>& rhs) noexcept {
  return lhs.error() == rhs.error();
}

/// Represents the result of a computation which can either complete
/// successfully with an instance of type `T` or fail with an instance of type
/// `E` (defaulting to `caf::error`).
/// @tparam T The type of the result.
/// @tparam E The type of the unexpected / error value.
template <class T, class E>
class expected {
public:
  // -- member types -----------------------------------------------------------

  using value_type = T;

  using error_type = E;

  using unexpected_type = unexpected<error_type>;

  template <class U>
  using rebind = expected<U, error_type>;

  // -- static member variables ------------------------------------------------

  /// Stores whether move construct and move assign never throw.
  static constexpr bool nothrow_move
    = std::is_nothrow_move_constructible_v<value_type>
      && std::is_nothrow_move_assignable_v<value_type>
      && std::is_nothrow_move_constructible_v<error_type>
      && std::is_nothrow_move_assignable_v<error_type>;

  /// Stores whether copy construct and copy assign never throw.
  static constexpr bool nothrow_copy
    = std::is_nothrow_copy_constructible_v<value_type>
      && std::is_nothrow_copy_assignable_v<value_type>
      && std::is_nothrow_copy_constructible_v<error_type>
      && std::is_nothrow_copy_assignable_v<error_type>;

  /// Stores whether swap() never throws.
  static constexpr bool nothrow_swap
    = std::is_nothrow_swappable_v<value_type>
      && std::is_nothrow_swappable_v<error_type>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class U>
    requires std::is_constructible_v<T, U>
  expected(U x) : has_value_{true} {
    new (std::addressof(value_)) T(std::move(x));
  }

  template <error_code_enum U>
    requires std::is_same_v<error_type, caf::error>
  CAF_DEPRECATED("construct using unexpect or from an unexpected instead")
  expected(U x) : has_value_{false} {
    new (std::addressof(error_)) caf::error(x);
  }

  expected(T&& x) noexcept(nothrow_move) : has_value_(true) {
    new (std::addressof(value_)) T(std::move(x));
  }

  expected(const T& x) noexcept(nothrow_copy) : has_value_(true) {
    new (std::addressof(value_)) T(x);
  }

  CAF_DEPRECATED("construct using unexpect or from an unexpected instead")
  expected(caf::error e) noexcept
    requires std::is_same_v<error_type, caf::error>
    : has_value_(false) {
    new (std::addressof(error_)) caf::error{std::move(e)};
  }

  expected(unexpected_type x) : has_value_(false) {
    new (std::addressof(error_)) error_type(std::move(x.error()));
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

  template <class... Ts>
  explicit expected(caf::unexpect_t, Ts&&... xs) : has_value_(false) {
    new (std::addressof(error_)) error_type(std::forward<Ts>(xs)...);
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
    requires std::is_constructible_v<T, U>
  expected& operator=(U x) {
    return *this = T{std::move(x)};
  }

  CAF_DEPRECATED("use assignment with caf::unexpected instead")
  expected& operator=(caf::error e) noexcept
    requires std::is_same_v<error_type, caf::error>
  {
    if (!has_value())
      error_ = std::move(e);
    else {
      destroy();
      has_value_ = false;
      new (std::addressof(error_)) caf::error(std::move(e));
    }
    return *this;
  }

  template <error_code_enum Enum>
    requires std::is_same_v<error_type, caf::error>
  CAF_DEPRECATED("use assignment with caf::unexpected instead")
  expected& operator=(Enum code) {
    return *this = make_error(code);
  }

  expected& operator=(unexpected_type e) noexcept(
    std::is_nothrow_move_assignable_v<error_type>) {
    if (!has_value())
      error_ = std::move(e.error());
    else {
      destroy();
      has_value_ = false;
      new (std::addressof(error_)) error_type(std::move(e.error()));
    }
    return *this;
  }

  // -- observers --------------------------------------------------------------

  /// @copydoc has_value
  explicit operator bool() const noexcept {
    return has_value_;
  }

  /// Returns `true` if the object holds a value (is engaged).
  bool has_value() const noexcept {
    return has_value_;
  }

  // -- modifiers --------------------------------------------------------------

  template <class... Args>
  T& emplace(Args&&... args) {
    destroy();
    has_value_ = true;
    new (std::addressof(value_)) T(std::forward<Args>(args)...);
    return value_;
  }

  void swap(expected& other) noexcept(nothrow_move && nothrow_swap) {
    expected tmp{std::move(other)};
    other = std::move(*this);
    *this = std::move(tmp);
  }

  // -- value access -----------------------------------------------------------

  /// Returns the contained value.
  /// @pre `has_value() == true`.
  T& value() & {
    if (!has_value())
      CAF_RAISE_ERROR("bad_expected_access");
    return value_;
  }

  /// @copydoc value
  const T& value() const& {
    if (!has_value())
      CAF_RAISE_ERROR("bad_expected_access");
    return value_;
  }

  /// @copydoc value
  T&& value() && {
    if (!has_value())
      CAF_RAISE_ERROR("bad_expected_access");
    return std::move(value_);
  }

  /// @copydoc value
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

  /// @copydoc value
  T& operator*() & noexcept {
    return value_;
  }

  /// @copydoc value
  const T& operator*() const& noexcept {
    return value_;
  }

  /// @copydoc value
  T&& operator*() && noexcept {
    return std::move(value_);
  }

  /// @copydoc value
  const T&& operator*() const&& noexcept {
    return std::move(value_);
  }

  /// @copydoc value
  T* operator->() noexcept {
    return &value_;
  }

  /// @copydoc value
  const T* operator->() const noexcept {
    return &value_;
  }

  // -- error access -----------------------------------------------------------

  /// Returns the contained error.
  /// @pre `has_value() == false`.
  error_type& error() & noexcept {
    CAF_ASSERT(!has_value_);
    return error_;
  }

  /// @copydoc error
  const error_type& error() const& noexcept {
    CAF_ASSERT(!has_value_);
    return error_;
  }

  /// @copydoc error
  error_type&& error() && noexcept {
    CAF_ASSERT(!has_value_);
    return std::move(error_);
  }

  /// @copydoc error
  const error_type&& error() const&& noexcept {
    CAF_ASSERT(!has_value_);
    return std::move(error_);
  }

  // -- monadic functions ------------------------------------------------------

  template <class F>
  auto and_then(F&& f) & {
    using res_t = decltype(f(value_));
    static_assert(detail::is_expected<res_t>, "F must return an expected");
    static_assert(
      std::is_same_v<error_type,
                     typename std::remove_cvref_t<res_t>::error_type>,
      "unexpected type must match");
    if (has_value())
      return f(value_);
    else
      return res_t{unexpect, error_};
  }

  template <class F>
  auto and_then(F&& f) && {
    using res_t = decltype(f(std::move(value_)));
    static_assert(detail::is_expected<res_t>, "F must return an expected");
    static_assert(
      std::is_same_v<error_type,
                     typename std::remove_cvref_t<res_t>::error_type>,
      "unexpected type must match");
    if (has_value())
      return f(std::move(value_));
    else
      return res_t{unexpect, std::move(error_)};
  }

  template <class F>
  auto and_then(F&& f) const& {
    using res_t = decltype(f(value_));
    static_assert(detail::is_expected<res_t>, "F must return an expected");
    static_assert(
      std::is_same_v<error_type,
                     typename std::remove_cvref_t<res_t>::error_type>,
      "unexpected type must match");
    if (has_value())
      return f(value_);
    else
      return res_t{unexpect, error_};
  }

  template <class F>
  auto and_then(F&& f) const&& {
    using res_t = decltype(f(std::move(value_)));
    static_assert(detail::is_expected<res_t>, "F must return an expected");
    static_assert(
      std::is_same_v<error_type,
                     typename std::remove_cvref_t<res_t>::error_type>,
      "unexpected type must match");
    if (has_value())
      return f(std::move(value_));
    else
      return res_t{unexpect, std::move(error_)};
  }

  template <class F>
    requires(std::is_same_v<error_type, caf::error>
             && std::is_void_v<
               decltype(std::declval<F&>()(std::declval<caf::error&>()))>)
  CAF_DEPRECATED("use or_else with expected return type instead")
  expected or_else(F&& f) & {
    if (!has_value())
      f(error_);
    return *this;
  }

  template <class F>
    requires std::is_same_v<
      expected<value_type, error_type>,
      decltype(std::declval<F&>()(std::declval<error_type&>()))>
  expected or_else(F&& f) & {
    if (has_value())
      return expected{std::in_place, value_};
    else
      return f(error_);
  }

  template <class F>
    requires(std::is_same_v<error_type, caf::error>
             && std::is_void_v<
               decltype(std::declval<F&>()(std::declval<caf::error &&>()))>)
  CAF_DEPRECATED("use or_else with expected return type instead")
  expected or_else(F&& f) && {
    if (!has_value())
      f(std::move(error_));
    return std::move(*this);
  }

  template <class F>
    requires std::is_same_v<
      expected<value_type, error_type>,
      decltype(std::declval<F&>()(std::declval<error_type&&>()))>
  expected or_else(F&& f) && {
    if (has_value())
      return expected{std::in_place, std::move(value_)};
    else
      return f(std::move(error_));
  }

  template <class F>
    requires(std::is_same_v<error_type, caf::error>
             && std::is_void_v<
               decltype(std::declval<F&>()(std::declval<const caf::error&>()))>)
  CAF_DEPRECATED("use or_else with expected return type instead")
  expected or_else(F&& f) const& {
    if (!has_value())
      f(error_);
    return *this;
  }

  template <class F>
    requires std::is_same_v<
      expected<value_type, error_type>,
      decltype(std::declval<F&>()(std::declval<const error_type&>()))>
  expected or_else(F&& f) const& {
    if (has_value())
      return expected{std::in_place, value_};
    else
      return f(error_);
  }

  template <class F>
    requires(std::is_same_v<error_type, caf::error>
             && std::is_void_v<decltype(std::declval<F&>()(
               std::declval<const caf::error &&>()))>)
  CAF_DEPRECATED("use or_else with expected return type instead")
  expected or_else(F&& f) const&& {
    if (!has_value())
      f(std::move(error_));
    return std::move(*this);
  }

  template <class F>
    requires std::is_same_v<
      expected<value_type, error_type>,
      decltype(std::declval<F&>()(std::declval<const error_type&&>()))>
  expected or_else(F&& f) const&& {
    if (has_value())
      return expected{std::in_place, std::move(value_)};
    else
      return f(std::move(error_));
  }

  template <class F>
  auto transform(F&& f) & {
    using res_t = decltype(f(value_));
    static_assert(!detail::is_expected<res_t>, "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn<error_type>(std::forward<F>(f), value_);
    else
      return expected<res_t, error_type>{unexpect, error_};
  }

  template <class F>
  auto transform(F&& f) && {
    using res_t = decltype(f(std::move(value_)));
    static_assert(!detail::is_expected<res_t>, "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn<error_type>(std::forward<F>(f),
                                                  std::move(value_));
    else
      return expected<res_t, error_type>{unexpect, std::move(error_)};
  }

  template <class F>
  auto transform(F&& f) const& {
    using res_t = decltype(f(value_));
    static_assert(!detail::is_expected<res_t>, "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn<error_type>(std::forward<F>(f), value_);
    else
      return expected<res_t, error_type>{unexpect, error_};
  }

  template <class F>
  auto transform(F&& f) const&& {
    using res_t = decltype(f(std::move(value_)));
    static_assert(!detail::is_expected<res_t>, "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn<error_type>(std::forward<F>(f),
                                                  std::move(value_));
    else
      return expected<res_t, error_type>{unexpect, std::move(error_)};
  }

  template <class F>
  CAF_DEPRECATED("use transform_error")
  expected transform_or(F&& f) & {
    return transform_error(std::forward<F>(f));
  }

  template <class F>
  CAF_DEPRECATED("use transform_error")
  expected transform_or(F&& f) && {
    return std::move(*this).transform_error(std::forward<F>(f));
  }

  template <class F>
  CAF_DEPRECATED("use transform_error")
  expected transform_or(F&& f) const& {
    return transform_error(std::forward<F>(f));
  }

  template <class F>
  CAF_DEPRECATED("use transform_error")
  expected transform_or(F&& f) const&& {
    return std::move(*this).transform_error(std::forward<F>(f));
  }

  template <class F>
  expected transform_error(F&& f) & {
    using res_t = decltype(f(error_));
    static_assert(std::is_same_v<error_type, res_t>,
                  "F must return an error_type value");
    if (has_value())
      return {std::in_place, value_};
    else
      return expected{unexpect, f(error_)};
  }

  template <class F>
  expected transform_error(F&& f) && {
    using res_t = decltype(f(std::move(error_)));
    static_assert(std::is_same_v<error_type, res_t>,
                  "F must return an error_type value");
    if (has_value())
      return {std::in_place, std::move(value_)};
    else
      return expected{unexpect, f(std::move(error_))};
  }

  template <class F>
  expected transform_error(F&& f) const& {
    using res_t = decltype(f(error_));
    static_assert(std::is_same_v<error_type, res_t>,
                  "F must return an error_type value");
    if (has_value())
      return {std::in_place, value_};
    else
      return expected{unexpect, f(error_)};
  }

  template <class F>
  expected transform_error(F&& f) const&& {
    using res_t = decltype(f(std::move(error_)));
    static_assert(std::is_same_v<error_type, res_t>,
                  "F must return an error_type value");
    if (has_value())
      return {std::in_place, std::move(value_)};
    else
      return expected{unexpect, f(std::move(error_))};
  }

private:
  void construct(expected&& other) noexcept(nothrow_move) {
    if (other.has_value_)
      new (std::addressof(value_)) T(std::move(other.value_));
    else
      new (std::addressof(error_)) error_type(std::move(other.error_));
    has_value_ = other.has_value_;
  }

  void construct(const expected& other) noexcept(nothrow_copy) {
    if (other.has_value_)
      new (std::addressof(value_)) T(other.value_);
    else
      new (std::addressof(error_)) error_type(other.error_);
    has_value_ = other.has_value_;
  }

  void destroy() {
    if (has_value())
      std::destroy_at(std::addressof(value_));
    else
      std::destroy_at(std::addressof(error_));
  }

  /// Denotes whether `value_` may be accessed. When false, error_ may be
  /// accessed.
  bool has_value_;

  union {
    /// Stores the contained value.
    T value_;

    /// Stores an error in case there is no value.
    error_type error_;
  };
};

/// @relates expected
template <class T, class E>
auto operator==(const expected<T, E>& x, const expected<T, E>& y)
  -> decltype(*x == *y) {
  return x && y ? *x == *y : (!x && !y ? x.error() == y.error() : false);
}

/// @relates expected
template <class T, class E, class U>
auto operator==(const expected<T, E>& x, const U& y) -> decltype(*x == y) {
  return x ? *x == y : false;
}

/// @relates expected
template <class U, class T, class E>
auto operator==(const U& x, const expected<T, E>& y) -> decltype(x == *y) {
  return y == x;
}

/// @relates expected
template <class T, class E>
  requires std::is_same_v<E, caf::error>
CAF_DEPRECATED("Compare with caf::unexpected{error} instead")
bool operator==(const expected<T, E>& x, const error& y) {
  return x ? false : x.error() == y;
}

/// @relates expected
template <class T, class E>
  requires std::is_same_v<E, caf::error>
CAF_DEPRECATED("Compare with caf::unexpected{error} instead")
bool operator==(const error& x, const expected<T, E>& y) {
  return y == x;
}

/// @relates expected
template <class T, class E, error_code_enum Enum>
  requires std::is_same_v<E, caf::error>
CAF_DEPRECATED("Compare with caf::unexpected{error{enum}} instead")
bool operator==(const expected<T, E>& x, Enum y) {
  return x == make_error(y);
}

/// @relates expected
template <error_code_enum Enum, class T, class E>
  requires std::is_same_v<E, caf::error>
CAF_DEPRECATED("Compare with caf::unexpected{error{enum}} instead")
bool operator==(Enum x, const expected<T, E>& y) {
  return y == make_error(x);
}

template <class T, class E>
bool operator==(const expected<T, E>& x, const unexpected<E>& y) {
  return x ? false : x.error() == y.error();
}

/// @relates expected
template <class E, class T>
bool operator==(const unexpected<E>& x, const expected<T, E>& y) {
  return y == x;
}

/// @relates expected
template <class T, class E>
auto operator!=(const expected<T, E>& x, const expected<T, E>& y)
  -> decltype(*x == *y) {
  return !(x == y);
}

/// @relates expected
template <class T, class E, class U>
auto operator!=(const expected<T, E>& x, const U& y) -> decltype(*x == y) {
  return !(x == y);
}

/// @relates expected
template <class U, class T, class E>
auto operator!=(const U& x, const expected<T, E>& y) -> decltype(x == *y) {
  return !(x == y);
}

/// @relates expected
template <class T, class E>
  requires std::is_same_v<E, caf::error>
CAF_DEPRECATED("Compare with caf::unexpected{error} instead")
bool operator!=(const expected<T, E>& x, const error& y) {
  return !(x == y);
}

/// @relates expected
template <class T, class E>
  requires std::is_same_v<E, caf::error>
CAF_DEPRECATED("Compare with caf::unexpected{error} instead")
bool operator!=(const error& x, const expected<T, E>& y) {
  return !(x == y);
}

/// @relates expected
template <class T, class E, error_code_enum Enum>
  requires std::is_same_v<E, caf::error>
CAF_DEPRECATED("Compare with caf::unexpected{error{enum}} instead")
bool operator!=(const expected<T, E>& x, Enum y) {
  return !(x == y);
}

/// @relates expected
template <error_code_enum Enum, class T, class E>
  requires std::is_same_v<E, caf::error>
CAF_DEPRECATED("Compare with caf::unexpected{error{enum}} instead")
bool operator!=(Enum x, const expected<T, E>& y) {
  return !(x == y);
}

/// @relates expected
template <class T, class E>
bool operator!=(const expected<T, E>& x, const unexpected<E>& y) {
  return !(x == y);
}

/// @relates expected
template <class E, class T>
bool operator!=(const unexpected<E>& x, const expected<T, E>& y) {
  return !(x == y);
}

/// The pattern `expected<void>` shall be used for functions that may generate
/// an error but would otherwise return `bool`.
template <class E>
class expected<void, E> {
public:
  // -- member types -----------------------------------------------------------

  using value_type = void;

  using error_type = E;

  using unexpected_type = unexpected<error_type>;

  template <class U>
  using rebind = expected<U, error_type>;

  // -- constructors, destructors, and assignment operators --------------------

  expected() noexcept = default;

  template <class U = error_type>
    requires(!std::is_same_v<error_type, caf::error>)
  explicit expected(error_type err) noexcept(
    std::is_nothrow_move_constructible_v<error_type>)
    : error_(std::move(err)) {
    // nop
  }

  template <error_code_enum Enum>
    requires std::is_same_v<error_type, caf::error>
  CAF_DEPRECATED("construct using unexpect or from an unexpected instead")
  expected(Enum x) : error_(make_error(x)) {
    // nop
  }

  template <class U = error_type>
    requires std::is_same_v<U, caf::error>
             && std::is_same_v<error_type, caf::error>
  CAF_DEPRECATED("construct using unexpect or from an unexpected instead")
  expected(caf::error err) noexcept : error_(std::move(err)) {
    // nop
  }

  expected(unexpected_type x) noexcept(
    std::is_nothrow_move_constructible_v<error_type>)
    : error_(std::move(x.error())) {
    // nop
  }

  expected(const expected& other) = default;

  expected(expected&& other) noexcept = default;

  explicit expected(std::in_place_t) {
    // nop
  }

  template <class... Args>
  explicit expected(unexpect_t, Args&&... args) {
    error_.emplace(std::forward<Args>(args)...);
  }

  expected& operator=(const expected& other) = default;

  expected& operator=(expected&& other) noexcept = default;

  template <class U = error_type>
    requires std::is_same_v<U, caf::error>
             && std::is_same_v<error_type, caf::error>
  CAF_DEPRECATED("use assignment with caf::unexpected instead")
  expected& operator=(caf::error err) noexcept {
    error_ = std::move(err);
    return *this;
  }

  template <error_code_enum Enum>
    requires std::is_same_v<error_type, caf::error>
  CAF_DEPRECATED("use assignment with caf::unexpected instead")
  expected& operator=(Enum code) {
    error_ = make_error(code);
    return *this;
  }

  expected& operator=(unexpected_type x) {
    error_ = std::move(x.error());
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

  error_type& error() & noexcept {
    CAF_ASSERT(!has_value());
    return *error_;
  }

  const error_type& error() const& noexcept {
    CAF_ASSERT(!has_value());
    return *error_;
  }

  error_type&& error() && noexcept {
    CAF_ASSERT(!has_value());
    return std::move(*error_);
  }

  const error_type&& error() const&& noexcept {
    CAF_ASSERT(!has_value());
    return std::move(*error_);
  }

  // -- monadic functions ------------------------------------------------------

  template <class F>
  auto and_then(F&& f) & {
    using res_t = decltype(f());
    static_assert(detail::is_expected<res_t>, "F must return an expected");
    static_assert(
      std::is_same_v<error_type,
                     typename std::remove_cvref_t<res_t>::error_type>,
      "unexpected type must match");
    if (has_value())
      return f();
    else
      return res_t{unexpect, *error_};
  }

  template <class F>
  auto and_then(F&& f) && {
    using res_t = decltype(f());
    static_assert(detail::is_expected<res_t>, "F must return an expected");
    static_assert(
      std::is_same_v<error_type,
                     typename std::remove_cvref_t<res_t>::error_type>,
      "unexpected type must match");
    if (has_value())
      return f();
    else
      return res_t{unexpect, std::move(*error_)};
  }

  template <class F>
  auto and_then(F&& f) const& {
    using res_t = decltype(f());
    static_assert(detail::is_expected<res_t>, "F must return an expected");
    static_assert(
      std::is_same_v<error_type,
                     typename std::remove_cvref_t<res_t>::error_type>,
      "unexpected type must match");
    if (has_value())
      return f();
    else
      return res_t{unexpect, *error_};
  }

  template <class F>
  auto and_then(F&& f) const&& {
    using res_t = decltype(f());
    static_assert(detail::is_expected<res_t>, "F must return an expected");
    static_assert(
      std::is_same_v<error_type,
                     typename std::remove_cvref_t<res_t>::error_type>,
      "unexpected type must match");
    if (has_value())
      return f();
    else
      return res_t{unexpect, std::move(*error_)};
  }

  template <class F>
  expected or_else(F&& f) & {
    using res_t = decltype(f(*error_));
    if constexpr (std::is_void_v<res_t>) {
      if (!has_value())
        f(*error_);
      return *this;
    } else {
      static_assert(std::is_same_v<expected<void, error_type>, res_t>,
                    "F must return expected<void, error_type> or void");
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
      static_assert(std::is_same_v<expected<void, error_type>, res_t>,
                    "F must return expected<void, error_type> or void");
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
      static_assert(std::is_same_v<expected<void, error_type>, res_t>,
                    "F must return expected<void, error_type> or void");
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
      static_assert(std::is_same_v<expected<void, error_type>, res_t>,
                    "F must return expected<void, error_type> or void");
      if (has_value())
        return expected{std::in_place};
      else
        return f(std::move(*error_));
    }
  }

  template <class F>
  auto transform(F&& f) & {
    using res_t = decltype(f());
    static_assert(!detail::is_expected<res_t>, "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn<error_type>(std::forward<F>(f));
    else
      return expected<res_t, error_type>{unexpect, *error_};
  }

  template <class F>
  auto transform(F&& f) && {
    using res_t = decltype(f());
    static_assert(!detail::is_expected<res_t>, "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn<error_type>(std::forward<F>(f));
    else
      return expected<res_t, error_type>{unexpect, std::move(*error_)};
  }

  template <class F>
  auto transform(F&& f) const& {
    using res_t = decltype(f());
    static_assert(!detail::is_expected<res_t>, "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn<error_type>(std::forward<F>(f));
    else
      return expected<res_t, error_type>{unexpect, *error_};
  }

  template <class F>
  auto transform(F&& f) const&& {
    using res_t = decltype(f());
    static_assert(!detail::is_expected<res_t>, "F must not return an expected");
    if (has_value())
      return detail::expected_from_fn<error_type>(std::forward<F>(f));
    else
      return expected<res_t, error_type>{unexpect, std::move(*error_)};
  }

  template <class F>
  CAF_DEPRECATED("use transform_error")
  expected transform_or(F&& f) & {
    return transform_error(std::forward<F>(f));
  }

  template <class F>
  CAF_DEPRECATED("use transform_error")
  expected transform_or(F&& f) && {
    return std::move(*this).transform_error(std::forward<F>(f));
  }

  template <class F>
  CAF_DEPRECATED("use transform_error")
  expected transform_or(F&& f) const& {
    return transform_error(std::forward<F>(f));
  }

  template <class F>
  CAF_DEPRECATED("use transform_error")
  expected transform_or(F&& f) const&& {
    return std::move(*this).transform_error(std::forward<F>(f));
  }

  template <class F>
  expected transform_error(F&& f) & {
    using res_t = decltype(f(*error_));
    static_assert(std::is_same_v<error_type, res_t>,
                  "F must return an error_type value");
    if (has_value())
      return expected{};
    else
      return expected{unexpect, f(*error_)};
  }

  template <class F>
  expected transform_error(F&& f) && {
    using res_t = decltype(f(std::move(*error_)));
    static_assert(std::is_same_v<error_type, res_t>,
                  "F must return an error_type value");
    if (has_value())
      return expected{};
    else
      return expected{unexpect, f(std::move(*error_))};
  }

  template <class F>
  expected transform_error(F&& f) const& {
    using res_t = decltype(f(*error_));
    static_assert(std::is_same_v<error_type, res_t>,
                  "F must return an error_type value");
    if (has_value())
      return expected{std::in_place};
    else
      return expected{unexpect, f(*error_)};
  }

  template <class F>
  expected transform_error(F&& f) const&& {
    using res_t = decltype(f(std::move(*error_)));
    static_assert(std::is_same_v<error_type, res_t>,
                  "F must return an error_type value");
    if (has_value())
      return expected{};
    else
      return expected{unexpect, f(std::move(*error_))};
  }

private:
  std::optional<error_type> error_;
};

/// @relates expected
template <class E>
bool operator==(const expected<void, E>& x, const expected<void, E>& y) {
  return (x && y) || (!x && !y && x.error() == y.error());
}

/// @relates expected
template <class E>
bool operator!=(const expected<void, E>& x, const expected<void, E>& y) {
  return !(x == y);
}

} // namespace caf

#endif

// NOLINTEND(bugprone-unchecked-optional-access)
