// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/async/fwd.hpp"
#include "caf/fwd.hpp"

#include <array>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace caf::detail {

template <class T>
constexpr T* null_v = nullptr;

template <class T, class... Ts>
concept one_of = (std::is_same_v<T, Ts> || ...);

/// Checks whether `T` is a `stream` or `typed_stream`.
template <class T>
struct is_stream_oracle : std::false_type {};

template <>
struct is_stream_oracle<stream> : std::true_type {};

template <class T>
struct is_stream_oracle<typed_stream<T>> : std::true_type {};

template <class T>
concept is_stream = is_stream_oracle<T>::value;

/// Checks whether  `T` is a `behavior` or `typed_behavior`.
template <class T>
struct is_behavior_oracle : std::false_type {};

template <>
struct is_behavior_oracle<behavior> : std::true_type {};

template <class... Ts>
struct is_behavior_oracle<typed_behavior<Ts...>> : std::true_type {};

template <class T>
concept is_behavior = is_behavior_oracle<T>::value;

template <class T>
concept has_make_behavior = requires(T& x) {
  { x.make_behavior() } -> is_behavior;
};

/// Checks whether `T` is a `publisher`.
template <class T>
struct is_publisher_oracle : std::false_type {};

template <class T>
struct is_publisher_oracle<async::publisher<T>> : std::true_type {};

template <class T>
concept is_publisher = is_publisher_oracle<T>::value;

/// Checks whether `T` defines a free function `to_string`.
template <class T>
concept has_to_string = requires(const T& x) {
  { to_string(x) } -> std::convertible_to<std::string>;
};

/// Checks whether `T` is primitive, i.e., either an arithmetic type or
/// convertible to one of STL's string types.
template <class T>
concept primitive = std::is_convertible_v<T, std::string>
                    || std::is_convertible_v<T, std::u16string>
                    || std::is_convertible_v<T, std::u32string>
                    || std::is_arithmetic_v<T>;

/// Checks whether `T1` is comparable with `T2`.
template <class T1, class T2>
concept is_comparable = requires(T1 lhs, T2 rhs) {
  { lhs == rhs } -> std::convertible_to<bool>;
  { lhs != rhs } -> std::convertible_to<bool>;
};

/// Checks whether `T` has `begin()` and `end()` member
/// functions returning forward iterators.
template <class T>
concept iterable = !primitive<T> && requires(T& t) {
  { t.begin() } -> std::forward_iterator;
  { t.begin() != t.end() } -> std::convertible_to<bool>;
};

/// Checks whether `T` is a non-const reference.
template <class T>
struct mutable_ref_oracle : std::false_type {};

template <class T>
struct mutable_ref_oracle<const T&> : std::false_type {};

template <class T>
struct mutable_ref_oracle<T&> : std::true_type {};

template <class T>
concept mutable_ref = mutable_ref_oracle<T>::value;

// Checks whether T has a member variable named `name`.
template <class T>
concept has_name = !std::is_scalar_v<T> && requires {
  { T::name } -> std::convertible_to<const char*>;
};

/// Checks whether F is convertible to either `std::function<void (T&)>`
/// or `std::function<void (const T&)>`.
template <class F, class T>
concept handler_for = requires(F& fn, T& val) {
  { fn(val) } -> std::same_as<void>;
} || requires(F& fn, const T& val) {
  { fn(val) } -> std::same_as<void>;
};

// Checks whether T has a member function named `push_back` that takes an
// element of type `T::value_type`.
template <class T>
concept has_push_back
  = requires(T& t) { t.push_back(std::declval<typename T::value_type>()); };

template <class T>
struct is_result_oracle : std::false_type {};

template <class T>
struct is_result_oracle<result<T>> : std::true_type {};

template <class T>
concept is_result = is_result_oracle<T>::value;

template <class T>
struct is_expected_oracle : std::false_type {};

template <class T>
struct is_expected_oracle<expected<T>> : std::true_type {};

template <class T>
concept is_expected = is_expected_oracle<T>::value;

/// Utility for fallbacks calling `static_assert`.
template <class>
struct always_false_oracle : std::false_type {};

template <class T>
concept always_false = always_false_oracle<T>::value;

/// Utility trait for checking whether T is a `std::pair`.
template <class T>
struct is_pair_oracle : std::false_type {};

template <class First, class Second>
struct is_pair_oracle<std::pair<First, Second>> : std::true_type {};

template <class T>
concept is_pair = is_pair_oracle<T>::value;

// -- traits to check for STL-style type aliases -------------------------------

template <class T>
concept has_value_type_alias = requires { typename T::value_type; };

template <class T>
concept has_key_type_alias = requires { typename T::key_type; };

template <class T>
concept has_mapped_type_alias = requires { typename T::mapped_type; };

template <class T>
concept has_handle_type_alias = requires { typename T::handle_type; };

// -- constexpr functions for use in require clauses ---------------------------

/// Checks whether T behaves like `std::map`.
template <class T>
concept map_like = iterable<T> && has_key_type_alias<T>
                   && has_mapped_type_alias<T>;

template <class T>
concept has_insert = requires(T& l, typename T::value_type& x) {
  { l.insert(l.end(), x) };
};

template <class T>
concept has_size = requires(T& t) {
  { t.size() } -> std::convertible_to<size_t>;
};

template <class T>
concept has_reserve = requires(T& t, size_t n) {
  { t.reserve(n) } -> std::same_as<void>;
};

template <class T>
concept has_emplace_back = requires(T& l, typename T::value_type& x) {
  { l.emplace_back(x) };
};

/// Checks whether T behaves like `std::vector`, `std::list`, or `std::set`.
template <class T>
concept list_like = iterable<T> && has_value_type_alias<T>
                    && !has_mapped_type_alias<T> && has_insert<T>
                    && has_size<T>;

template <class T, class To>
concept has_convertible_data_member = requires(T& x) {
  { x.data() } -> std::convertible_to<To*>;
};

/// Evaluates to `true` for all types that specialize `std::tuple_size`, i.e.,
/// `std::tuple`, `std::pair`, and `std::array`.
template <class T>
concept specializes_tuple_size = requires(T&) {
  { std::tuple_size<T>::value } -> std::convertible_to<size_t>;
};

template <class Inspector>
concept has_context = requires(Inspector& f) {
  { f.context() } -> std::same_as<scheduler*>;
};

/// Checks whether `T` provides an `inspect` overload for `Inspector`.
template <class Inspector, class T>
concept has_inspect_overload = requires(Inspector& x, T& y) {
  { inspect(x, y) } -> std::same_as<bool>;
};

/// Checks whether the inspector has a `builtin_inspect` overload for `T`.
template <class Inspector, class T>
concept has_builtin_inspect = requires(Inspector& f, T& x) {
  { f.builtin_inspect(x) } -> std::same_as<bool>;
};

/// Checks whether the inspector has an `opaque_value` overload for `T`.
template <class Inspector, class T>
concept accepts_opaque_value = requires(Inspector& f, T& x) {
  { f.opaque_value(x) } -> std::same_as<bool>;
};

/// Type trait that checks whether `T` is a built-in type for the inspector,
/// i.e., an arithmetic type or a type that explicitly specializes the trait.
template <class T, bool IsLoading>
struct is_builtin_inspector_type_oracle {
  static constexpr bool value = std::is_arithmetic_v<T>;
};

template <bool IsLoading>
struct is_builtin_inspector_type_oracle<std::byte, IsLoading> {
  static constexpr bool value = true;
};

template <bool IsLoading>
struct is_builtin_inspector_type_oracle<byte_span, IsLoading> {
  static constexpr bool value = true;
};

template <bool IsLoading>
struct is_builtin_inspector_type_oracle<std::string, IsLoading> {
  static constexpr bool value = true;
};

template <bool IsLoading>
struct is_builtin_inspector_type_oracle<std::u16string, IsLoading> {
  static constexpr bool value = true;
};

template <bool IsLoading>
struct is_builtin_inspector_type_oracle<std::u32string, IsLoading> {
  static constexpr bool value = true;
};

template <bool IsLoading>
struct is_builtin_inspector_type_oracle<strong_actor_ptr, IsLoading> {
  static constexpr bool value = true;
};

template <bool IsLoading>
struct is_builtin_inspector_type_oracle<weak_actor_ptr, IsLoading> {
  static constexpr bool value = true;
};

template <>
struct is_builtin_inspector_type_oracle<std::string_view, false> {
  static constexpr bool value = true;
};

template <>
struct is_builtin_inspector_type_oracle<const_byte_span, false> {
  static constexpr bool value = true;
};

template <class Inspector, class T>
concept is_builtin_inspector_type
  = is_builtin_inspector_type_oracle<T, Inspector::is_loading>::value;

/// Checks whether `T` is a `std::optional`.
template <class T>
struct is_optional_oracle : std::false_type {};

template <class T>
struct is_optional_oracle<std::optional<T>> : std::true_type {};

template <class T>
concept is_optional = is_optional_oracle<T>::value;

template <class T>
struct unboxed_trait {
  using type = T;
};

template <class T>
struct unboxed_trait<std::optional<T>> {
  using type = T;
};

template <class T>
struct unboxed_trait<expected<T>> {
  using type = T;
};

template <class T>
using unboxed = typename unboxed_trait<T>::type;

template <class T>
concept is_64bit_integer = std::is_same_v<T, int64_t>
                           || std::is_same_v<T, uint64_t>;

/// Checks whether `T` has a static member function called `init_host_system`.
template <class T>
concept has_init_host_system = requires {
  { T::init_host_system() };
};

/// Drop-in replacement for C++23's std::to_underlying.
template <class Enum>
[[nodiscard]] constexpr auto to_underlying(Enum e) noexcept {
  return static_cast<std::underlying_type_t<Enum>>(e);
}

} // namespace caf::detail
