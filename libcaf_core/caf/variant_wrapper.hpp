// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>
#include <variant>

namespace caf {

/// Customization point. Any type that declares itself as a variant wrapper
/// enables `visit`, `holds_alternative`, `get` and `get_if`. Each type must
/// provide a member function `get_data()` to expose the internal
/// `std::variant`.
template <class>
struct is_variant_wrapper : std::false_type {};

/// @relates is_variant_wrapper
template <class T>
constexpr bool is_variant_wrapper_v = is_variant_wrapper<T>::value;

/// @relates is_variant_wrapper
template <class T>
using enable_variant_wrapper_t = std::enable_if_t<is_variant_wrapper_v<T>>;

/// @relates is_variant_wrapper
template <class F, class V, class = enable_variant_wrapper_t<std::decay_t<V>>>
decltype(auto) visit(F&& f, V&& x) {
  return std::visit(std::forward<F>(f), std::forward<V>(x).get_data());
}

/// @relates is_variant_wrapper
template <class T, class V, class = enable_variant_wrapper_t<V>>
bool holds_alternative(const V& x) noexcept {
  return std::holds_alternative<T>(x.get_data());
}

/// @relates is_variant_wrapper
template <class T, class V, class = enable_variant_wrapper_t<V>>
T& get(V& x) {
  return std::get<T>(x.get_data());
}

/// @relates is_variant_wrapper
template <class T, class V, class = enable_variant_wrapper_t<V>>
const T& get(const V& x) {
  return std::get<T>(x.get_data());
}

/// @relates is_variant_wrapper
template <class T, class V, class = enable_variant_wrapper_t<V>>
T* get_if(V* ptr) noexcept {
  return std::get_if<T>(&ptr->get_data());
}

/// @relates is_variant_wrapper
template <class T, class V, class = enable_variant_wrapper_t<V>>
const T* get_if(const V* ptr) noexcept {
  return std::get_if<T>(&ptr->get_data());
}

} // namespace caf
