/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <chrono>
#include <memory>
#include <tuple>
#include <utility>

#ifdef __has_include
#  if __has_include(<optional>)
#    include <optional>
#    if __cpp_lib_optional >= 201606
#      define CAF_HAS_STD_OPTIONAL
#    endif
#  endif
#  if __has_include(<variant>)
#    include <variant>
#    if __cpp_lib_variant >= 201606
#      define CAF_HAS_STD_VARIANT
#    endif
#  endif
#endif

#include "caf/allowed_unsafe_message_type.hpp"
#include "caf/detail/as_mutable_ref.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/print.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector_access_base.hpp"
#include "caf/inspector_access_type.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/optional.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/sum_type_access.hpp"
#include "caf/variant.hpp"

namespace caf::detail {

// -- predicates ---------------------------------------------------------------

/// Utility class for predicates that always return `true`.
struct always_true_t {
  template <class... Ts>
  [[nodiscard]] constexpr bool operator()(Ts&&...) const noexcept {
    return true;
  }
};

/// Predicate that always returns `true`.
constexpr auto always_true = always_true_t{};

// -- traits -------------------------------------------------------------------

template <class>
constexpr bool assertion_failed_v = false;

// -- loading ------------------------------------------------------------------

template <class Inspector, class T>
bool load_value(Inspector& f, T& x);

template <class Inspector, class T>
bool load_value(Inspector& f, T& x, inspector_access_type::specialization) {
  return inspector_access<T>::apply_value(f, x);
}

template <class Inspector, class T>
bool load_value(Inspector& f, T& x, inspector_access_type::inspect_value) {
  return inspect_value(f, x);
}

template <class Inspector, class T>
bool load_value(Inspector& f, T& x, inspector_access_type::inspect) {
  return inspect(f, x);
}

template <class Inspector, class T>
bool load_value(Inspector& f, T& x, inspector_access_type::integral) {
  auto tmp = detail::squashed_int_t<T>{0};
  if (f.value(tmp)) {
    x = static_cast<T>(tmp);
    return true;
  }
  return false;
}

template <class Inspector, class T>
bool load_value(Inspector& f, T& x, inspector_access_type::builtin) {
  return f.value(x);
}

template <class Inspector, class T>
bool load_value(Inspector& f, T& x, inspector_access_type::enumeration) {
  auto tmp = detail::squashed_int_t<std::underlying_type_t<T>>{0};
  if (f.value(tmp)) {
    x = static_cast<T>(tmp);
    return true;
  }
  return false;
}

template <class Inspector, class T>
bool load_value(Inspector& f, T& x, inspector_access_type::empty) {
  return f.object(x).fields();
}

template <class Inspector, class T>
bool load_value(Inspector& f, T&, inspector_access_type::unsafe) {
  f.emplace_error(sec::unsafe_type);
  return false;
}

template <class Inspector, class T, size_t N>
bool load_value(Inspector& f, T (&xs)[N], inspector_access_type::array) {
  if (!f.begin_tuple(N))
    return false;
  for (size_t index = 0; index < N; ++index)
    if (!load_value(f, xs[index]))
      return false;
  return f.end_tuple();
}

template <class Inspector, class T, size_t... Ns>
bool load_tuple(Inspector& f, T& xs, std::index_sequence<Ns...>) {
  return f.begin_tuple(sizeof...(Ns))           //
         && (load_value(f, get<Ns>(xs)) && ...) //
         && f.end_tuple();
}

template <class Inspector, class T>
bool load_value(Inspector& f, T& x, inspector_access_type::tuple) {
  return load_tuple(f, x,
                    std::make_index_sequence<std::tuple_size<T>::value>{});
}

template <class Inspector, class T>
bool load_value(Inspector& f, T& x, inspector_access_type::map) {
  x.clear();
  size_t size = 0;
  if (!f.begin_associative_array(size))
    return false;
  for (size_t i = 0; i < size; ++i) {
    auto key = typename T::key_type{};
    auto val = typename T::mapped_type{};
    if (!(f.begin_key_value_pair() //
          && load_value(f, key)    //
          && load_value(f, val)    //
          && f.end_key_value_pair()))
      return false;
    if (!x.emplace(std::move(key), std::move(val)).second) {
      f.emplace_error(sec::runtime_error, "multiple key definitions");
      return false;
    }
  }
  return f.end_associative_array();
}

template <class Inspector, class T>
bool load_value(Inspector& f, T& x, inspector_access_type::list) {
  x.clear();
  size_t size = 0;
  if (!f.begin_sequence(size))
    return false;
  for (size_t i = 0; i < size; ++i) {
    auto val = typename T::value_type{};
    if (!detail::load_value(f, val))
      return false;
    x.insert(x.end(), std::move(val));
  }
  return f.end_sequence();
}

template <class Inspector, class T>
std::enable_if_t<accepts_opaque_value<Inspector, T>::value, bool>
load_value(Inspector& f, T& x, inspector_access_type::none) {
  return f.opaque_value(x);
}

template <class Inspector, class T>
std::enable_if_t<!accepts_opaque_value<Inspector, T>::value, bool>
load_value(Inspector&, T&, inspector_access_type::none) {
  static_assert(
    detail::assertion_failed_v<T>,
    "please provide an inspect overload for T or specialize inspector_access");
  return false;
}

template <class Inspector, class T>
bool load_value(Inspector& f, T& x) {
  return load_value(f, x, inspect_value_access_type<Inspector, T>());
}

template <class Inspector, class T>
bool load_object(Inspector& f, T& x, inspector_access_type::specialization) {
  return inspector_access<T>::apply_object(f, x);
}

template <class Inspector, class T>
bool load_object(Inspector& f, T& x, inspector_access_type::inspect) {
  return inspect(f, x);
}

template <class Inspector, class T, class Token>
bool load_object(Inspector& f, T& x, Token) {
  return f.object(x).fields(f.field("value", x));
}

template <class Inspector, class T>
bool load_object(Inspector& f, T& x) {
  return load_object(f, x, inspect_object_access_type<Inspector, T>());
}

template <class Inspector, class T, class IsValid, class SyncValue>
bool load_field(Inspector& f, string_view field_name, T& x, IsValid& is_valid,
                SyncValue& sync_value) {
  using impl = std::conditional_t<is_complete<inspector_access<T>>, // if
                                  inspector_access<T>,              // then
                                  inspector_access_base<T>>;        // else
  return impl::load_field(f, field_name, x, is_valid, sync_value);
}

template <class Inspector, class T, class IsValid, class SyncValue,
          class SetFallback>
bool load_field(Inspector& f, string_view field_name, T& x, IsValid& is_valid,
                SyncValue& sync_value, SetFallback& set_fallback) {
  using impl = std::conditional_t<is_complete<inspector_access<T>>, // if
                                  inspector_access<T>,              // then
                                  inspector_access_base<T>>;        // else
  return impl::load_field(f, field_name, x, is_valid, sync_value, set_fallback);
}

// -- saving -------------------------------------------------------------------

template <class Inspector, class T>
bool save_value(Inspector& f, T& x);

template <class Inspector, class T>
bool save_value(Inspector& f, const T& x);

template <class Inspector, class T>
bool save_value(Inspector& f, T& x, inspector_access_type::specialization) {
  return inspector_access<T>::apply_value(f, x);
}

template <class Inspector, class T>
bool save_value(Inspector& f, T& x, inspector_access_type::inspect_value) {
  return inspect_value(f, x);
}

template <class Inspector, class T>
bool save_value(Inspector& f, T& x, inspector_access_type::inspect) {
  return inspect(f, x);
}

template <class Inspector, class T>
bool save_value(Inspector& f, T& x, inspector_access_type::integral) {
  auto tmp = static_cast<detail::squashed_int_t<T>>(x);
  return f.value(tmp);
}

template <class Inspector, class T>
bool save_value(Inspector& f, T& x, inspector_access_type::builtin) {
  return f.value(x);
}

template <class Inspector, class T>
bool save_value(Inspector& f, T& x, inspector_access_type::enumeration) {
  auto tmp = static_cast<detail::squashed_int_t<std::underlying_type_t<T>>>(x);
  return f.value(tmp);
}

template <class Inspector, class T>
bool save_value(Inspector& f, T& x, inspector_access_type::empty) {
  return f.object(x).fields();
}

template <class Inspector, class T>
bool save_value(Inspector& f, T&, inspector_access_type::unsafe) {
  f.emplace_error(sec::unsafe_type);
  return false;
}

template <class Inspector, class T, size_t N>
bool save_value(Inspector& f, T (&xs)[N], inspector_access_type::array) {
  if (!f.begin_tuple(N))
    return false;
  for (size_t index = 0; index < N; ++index)
    if (!save_value(f, xs[index]))
      return false;
  return f.end_tuple();
}

template <class Inspector, class T, size_t... Ns>
bool save_tuple(Inspector& f, T& xs, std::index_sequence<Ns...>) {
  return f.begin_tuple(sizeof...(Ns))           //
         && (save_value(f, get<Ns>(xs)) && ...) //
         && f.end_tuple();
}

template <class Inspector, class T>
bool save_value(Inspector& f, T& x, inspector_access_type::tuple) {
  return save_tuple(f, x,
                    std::make_index_sequence<std::tuple_size<T>::value>{});
}

template <class Inspector, class T>
bool save_value(Inspector& f, T& x, inspector_access_type::map) {
  if (!f.begin_associative_array(x.size()))
    return false;
  for (auto&& kvp : x) {
    if (!(f.begin_key_value_pair()     //
          && save_value(f, kvp.first)  //
          && save_value(f, kvp.second) //
          && f.end_key_value_pair()))
      return false;
  }
  return f.end_associative_array();
}

template <class Inspector, class T>
bool save_value(Inspector& f, T& x, inspector_access_type::list) {
  auto size = x.size();
  if (!f.begin_sequence(size))
    return false;
  for (auto&& val : x)
    if (!save_value(f, val))
      return false;
  return f.end_sequence();
}

template <class Inspector, class T>
std::enable_if_t<accepts_opaque_value<Inspector, T>::value, bool>
save_value(Inspector& f, T& x, inspector_access_type::none) {
  return f.opaque_value(x);
}

template <class Inspector, class T>
std::enable_if_t<!accepts_opaque_value<Inspector, T>::value, bool>
save_value(Inspector&, T&, inspector_access_type::none) {
  static_assert(
    detail::assertion_failed_v<T>,
    "please provide an inspect overload for T or specialize inspector_access");
  return false;
}

template <class Inspector, class T>
bool save_value(Inspector& f, T& x) {
  return save_value(f, x, inspect_value_access_type<Inspector, T>());
}

template <class Inspector, class T>
bool save_value(Inspector& f, const T& x) {
  return save_value(f, as_mutable_ref(x),
                    inspect_value_access_type<Inspector, T>());
}

template <class Inspector, class T>
bool save_object(Inspector& f, T& x, inspector_access_type::specialization) {
  return inspector_access<T>::apply_object(f, x);
}

template <class Inspector, class T>
bool save_object(Inspector& f, T& x, inspector_access_type::inspect) {
  return inspect(f, x);
}

template <class Inspector, class T, class Token>
bool save_object(Inspector& f, T& x, Token) {
  return f.object(x).fields(f.field("value", x));
}

template <class Inspector, class T>
bool save_object(Inspector& f, T& x) {
  return save_object(f, x, inspect_object_access_type<Inspector, T>());
}

template <class Inspector, class T>
bool save_field(Inspector& f, string_view field_name, T& x) {
  using impl = std::conditional_t<is_complete<inspector_access<T>>, // if
                                  inspector_access<T>,              // then
                                  inspector_access_base<T>>;        // else
  return impl::save_field(f, field_name, x);
}

template <class Inspector, class IsPresent, class Get>
bool save_field(Inspector& f, string_view field_name, IsPresent& is_present,
                Get& get) {
  using T = std::decay_t<decltype(get())>;
  using impl = std::conditional_t<is_complete<inspector_access<T>>, // if
                                  inspector_access<T>,              // then
                                  inspector_access_base<T>>;        // else
  return impl::save_field(f, field_name, is_present, get);
}

// -- dispatching to save or load using getter/setter API ----------------------

template <class Inspector, class T>
bool split_save_load(Inspector& f, T& x) {
  if constexpr (Inspector::is_loading)
    return load_value(f, x);
  else
    return save_value(f, x);
}

template <class Inspector, class Get, class Set>
bool split_save_load(Inspector& f, Get&& get, Set&& set) {
  using value_type = std::decay_t<decltype(get())>;
  if constexpr (Inspector::is_loading) {
    auto tmp = value_type{};
    if (load_value(f, tmp))
      return set(std::move(tmp));
    return false;
  } else {
    auto&& x = get();
    return save_value(f, x);
  }
}

} // namespace caf::detail

namespace caf {

/// Default implementation for @ref inspector_access.
template <class T>
struct default_inspector_access : inspector_access_base<T> {
  // -- interface functions ----------------------------------------------------

  /// Applies `x` as an object to `f`.
  template <class Inspector>
  [[nodiscard]] static bool apply_object(Inspector& f, T& x) {
    // Dispatch to user-provided `inspect` overload or assume a trivial type.
    if constexpr (detail::has_inspect_overload<Inspector, T>::value) {
      using result_type = decltype(inspect(f, x));
      if constexpr (std::is_same<result_type, bool>::value)
        return inspect(f, x);
      else
        return apply_deprecated(f, x);
    } else if constexpr (std::is_empty<T>::value) {
      return f.object(x).fields();
    } else {
      return f.object(x).fields(f.field("value", x));
    }
  }

  /// Applies `x` as a single value to `f`.
  template <class Inspector>
  [[nodiscard]] static bool apply_value(Inspector& f, T& x) {
    constexpr auto token = inspect_value_access_type<Inspector, T>();
    if constexpr (Inspector::is_loading)
      return detail::load_value(f, x, token);
    else
      return detail::save_value(f, x, token);
  }

  // -- deprecated API ---------------------------------------------------------

  template <class Inspector>
  [[deprecated("inspect() overloads should return bool")]] //
  [[nodiscard]] static bool
  apply_deprecated(Inspector& f, T& x) {
    if (auto err = inspect(f, x)) {
      f.emplace_error(std::move(err));
      return false;
    }
    return true;
  }
};

/// Customization point for adding support for a custom type. The default
/// implementation requires an `inspect` overload for `T` that is available via
/// ADL.
template <class T>
struct inspector_access;

// -- inspection support for optional values -----------------------------------

template <class T>
struct optional_inspector_traits;

template <class T>
struct optional_inspector_traits<optional<T>> {
  using container_type = optional<T>;

  using value_type = T;

  template <class... Ts>
  static void emplace(container_type& container, Ts&&... xs) {
    container.emplace(std::forward<Ts>(xs)...);
  }
};

template <class T>
struct optional_inspector_traits<intrusive_ptr<T>> {
  using container_type = intrusive_ptr<T>;

  using value_type = T;

  template <class... Ts>
  static void emplace(container_type& container, Ts&&... xs) {
    container.reset(new T(std::forward<Ts>(xs)...));
  }
};

template <class T>
struct optional_inspector_traits<std::unique_ptr<T>> {
  using container_type = std::unique_ptr<T>;

  using value_type = T;

  template <class... Ts>
  static void emplace(container_type& container, Ts&&... xs) {
    container = std::make_unique<T>(std::forward<Ts>(xs)...);
  }
};

template <class T>
struct optional_inspector_traits<std::shared_ptr<T>> {
  using container_type = std::shared_ptr<T>;

  using value_type = T;

  template <class... Ts>
  static void emplace(container_type& container, Ts&&... xs) {
    container = std::make_shared<T>(std::forward<Ts>(xs)...);
  }
};

/// Provides inspector access for types that represent optional values.
template <class T>
struct optional_inspector_access {
  using traits = optional_inspector_traits<T>;

  using container_type = typename traits::container_type;

  using value_type = typename traits::value_type;

  template <class Inspector>
  [[nodiscard]] static bool apply_object(Inspector& f, container_type& x) {
    return f.object(x).fields(f.field("value", x));
  }

  template <class Inspector>
  [[nodiscard]] static bool apply_value(Inspector& f, container_type& x) {
    return apply_object(f, x);
  }

  template <class Inspector>
  static bool
  save_field(Inspector& f, string_view field_name, container_type& x) {
    auto is_present = [&x] { return static_cast<bool>(x); };
    auto get = [&x]() -> decltype(auto) { return *x; };
    return detail::save_field(f, field_name, is_present, get);
  }

  template <class Inspector, class IsPresent, class Get>
  static bool save_field(Inspector& f, string_view field_name,
                         IsPresent& is_present, Get& get) {
    return detail::save_field(f, field_name, is_present, get);
  }

  template <class Inspector, class IsValid, class SyncValue>
  static bool load_field(Inspector& f, string_view field_name,
                         container_type& x, IsValid& is_valid,
                         SyncValue& sync_value) {
    traits::emplace(x);
    auto set_x = [&] { return sync_value(); };
    auto reset = [&x] { x.reset(); };
    return detail::load_field(f, field_name, *x, is_valid, set_x, reset);
  }

  template <class Inspector, class IsValid, class SyncValue, class SetFallback>
  static bool load_field(Inspector& f, string_view field_name,
                         container_type& x, IsValid& is_valid,
                         SyncValue& sync_value, SetFallback& set_fallback) {
    traits::emplace(x);
    auto set_x = [&] { return sync_value(); };
    return detail::load_field(f, field_name, *x, is_valid, set_x, set_fallback);
  }
};

// -- inspection support for optional<T> ---------------------------------------

template <class T>
struct inspector_access<optional<T>> : optional_inspector_access<optional<T>> {
  // nop
};

#ifdef CAF_HAS_STD_OPTIONAL

template <class T>
struct optional_inspector_traits<std::optional<T>> {
  using container_type = std::optional<T>;

  using value_type = T;

  template <class... Ts>
  static void emplace(container_type& container, Ts&&... xs) {
    container.emplace(std::forward<Ts>(xs)...);
  }
};

template <class T>
struct inspector_access<std::optional<T>>
  : optional_inspector_access<std::optional<T>> {
  // nop
};

#endif

// -- inspection support for error ---------------------------------------------

template <>
struct inspector_access<std::unique_ptr<error::data>>
  : optional_inspector_access<std::unique_ptr<error::data>> {
  // nop
};

// -- inspection support for variant<Ts...> ------------------------------------

template <class T>
struct variant_inspector_traits;

template <class... Ts>
struct variant_inspector_traits<variant<Ts...>> {
  static_assert(
    (has_type_id_v<Ts> && ...),
    "inspectors requires that each type in a variant has a type_id");

  using value_type = variant<Ts...>;

  static constexpr type_id_t allowed_types[] = {type_id_v<Ts>...};

  static auto type_index(const value_type& x) {
    return x.index();
  }
  template <class F, class Value>
  static auto visit(F&& f, Value&& x) {
    return caf::visit(std::forward<F>(f), std::forward<Value>(x));
  }

  template <class U>
  static auto assign(value_type& x, U&& value) {
    x = std::forward<U>(value);
  }

  template <class F>
  static bool load(type_id_t, F&, detail::type_list<>) {
    return false;
  }

  template <class F, class U, class... Us>
  static bool
  load(type_id_t type, F& continuation, detail::type_list<U, Us...>) {
    if (type_id_v<U> == type) {
      auto tmp = U{};
      continuation(tmp);
      return true;
    }
    return load(type, continuation, detail::type_list<Us...>{});
  }

  template <class F>
  static bool load(type_id_t type, F continuation) {
    return load(type, continuation, detail::type_list<Ts...>{});
  }
};

template <class T>
struct variant_inspector_access {
  using value_type = T;

  using traits = variant_inspector_traits<T>;

  template <class Inspector>
  [[nodiscard]] static bool apply_object(Inspector& f, value_type& x) {
    return f.object(x).fields(f.field("value", x));
  }

  template <class Inspector>
  [[nodiscard]] static bool apply_value(Inspector& f, value_type& x) {
    return apply_object(f, x);
  }

  template <class Inspector>
  static bool save_field(Inspector& f, string_view field_name, value_type& x) {
    auto g = [&f](auto& y) { return detail::save_value(f, y); };
    return f.begin_field(field_name, make_span(traits::allowed_types),
                         traits::type_index(x)) //
           && traits::visit(g, x)               //
           && f.end_field();
  }

  template <class Inspector, class IsPresent, class Get>
  static bool save_field(Inspector& f, string_view field_name,
                         IsPresent& is_present, Get& get) {
    auto allowed_types = make_span(traits::allowed_types);
    if (is_present()) {
      auto&& x = get();
      auto g = [&f](auto& y) { return detail::save_value(f, y); };
      return f.begin_field(field_name, true, allowed_types,
                           traits::type_index(x)) //
             && traits::visit(g, x)               //
             && f.end_field();
    }
    return f.begin_field(field_name, false, allowed_types, 0) //
           && f.end_field();
  }

  template <class Inspector>
  static bool load_variant_value(Inspector& f, string_view field_name,
                                 value_type& x, type_id_t runtime_type) {
    auto res = false;
    auto type_found = traits::load(runtime_type, [&](auto& tmp) {
      if (!detail::load_value(f, tmp))
        return;
      traits::assign(x, std::move(tmp));
      res = true;
      return;
    });
    if (!type_found)
      f.emplace_error(sec::invalid_field_type, to_string(field_name));
    return res;
  }

  template <class Inspector, class IsValid, class SyncValue>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value) {
    size_t type_index = std::numeric_limits<size_t>::max();
    auto allowed_types = make_span(traits::allowed_types);
    if (!f.begin_field(field_name, allowed_types, type_index))
      return false;
    if (type_index >= allowed_types.size()) {
      f.emplace_error(sec::invalid_field_type, to_string(field_name));
      return false;
    }
    auto runtime_type = allowed_types[type_index];
    if (!load_variant_value(f, field_name, x, runtime_type))
      return false;
    if (!is_valid(x)) {
      f.emplace_error(sec::field_invariant_check_failed, to_string(field_name));
      return false;
    }
    if (!sync_value()) {
      f.emplace_error(sec::field_value_synchronization_failed,
                      to_string(field_name));
      return false;
    }
    return f.end_field();
  }

  template <class Inspector, class IsValid, class SyncValue, class SetFallback>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value,
                         SetFallback& set_fallback) {
    bool is_present = false;
    auto allowed_types = make_span(traits::allowed_types);
    size_t type_index = std::numeric_limits<size_t>::max();
    if (!f.begin_field(field_name, is_present, allowed_types, type_index))
      return false;
    if (is_present) {
      if (type_index >= allowed_types.size()) {
        f.emplace_error(sec::invalid_field_type, to_string(field_name));
        return false;
      }
      auto runtime_type = allowed_types[type_index];
      if (!load_variant_value(f, field_name, x, runtime_type))
        return false;
      if (!is_valid(x)) {
        f.emplace_error(sec::field_invariant_check_failed,
                        to_string(field_name));
        return false;
      }
      if (!sync_value()) {
        f.emplace_error(sec::field_value_synchronization_failed,
                        to_string(field_name));
        return false;
      }
    } else {
      set_fallback();
    }
    return f.end_field();
  }
};

template <class... Ts>
struct inspector_access<variant<Ts...>>
  : variant_inspector_access<variant<Ts...>> {
  // nop
};

#ifdef CAF_HAS_STD_VARIANT

template <class... Ts>
struct variant_inspector_traits<std::variant<Ts...>> {
  static_assert(
    (has_type_id_v<Ts> && ...),
    "inspectors requires that each type in a variant has a type_id");

  using value_type = std::variant<Ts...>;

  static constexpr type_id_t allowed_types[] = {type_id_v<Ts>...};

  static auto type_index(const value_type& x) {
    return x.index();
  }

  template <class F, class Value>
  static auto visit(F&& f, Value&& x) {
    return std::visit(std::forward<F>(f), std::forward<Value>(x));
  }

  template <class U>
  static auto assign(value_type& x, U&& value) {
    x = std::forward<U>(value);
  }

  template <class F>
  static bool load(type_id_t, F&, detail::type_list<>) {
    return false;
  }

  template <class F, class U, class... Us>
  static bool
  load(type_id_t type, F& continuation, detail::type_list<U, Us...>) {
    if (type_id_v<U> == type) {
      auto tmp = U{};
      continuation(tmp);
      return true;
    }
    return load(type, continuation, detail::type_list<Us...>{});
  }

  template <class F>
  static bool load(type_id_t type, F continuation) {
    return load(type, continuation, detail::type_list<Ts...>{});
  }
};

template <class... Ts>
struct inspector_access<std::variant<Ts...>>
  : variant_inspector_access<std::variant<Ts...>> {
  // nop
};

#endif

// -- inspection support for std::chrono types ---------------------------------

template <class Rep, class Period>
struct inspector_access<std::chrono::duration<Rep, Period>>
  : inspector_access_base<std::chrono::duration<Rep, Period>> {
  using value_type = std::chrono::duration<Rep, Period>;

  using default_impl = default_inspector_access<value_type>;

  template <class Inspector>
  static bool apply_object(Inspector& f, value_type& x) {
    return f.object(x).fields(f.field("value", x));
  }

  template <class Inspector>
  static bool apply_value(Inspector& f, value_type& x) {
    if (f.has_human_readable_format()) {
      auto get = [&x] {
        std::string str;
        detail::print(str, x);
        return str;
      };
      auto set = [&x](std::string str) {
        auto err = detail::parse(str, x);
        return !err;
      };
      return detail::split_save_load(f, get, set);
    }
    auto get = [&x] { return x.count(); };
    auto set = [&x](Rep value) {
      x = std::chrono::duration<Rep, Period>{value};
      return true;
    };
    return detail::split_save_load(f, get, set);
  }
};

template <class Duration>
struct inspector_access<
  std::chrono::time_point<std::chrono::system_clock, Duration>>
  : inspector_access_base<
      std::chrono::time_point<std::chrono::system_clock, Duration>> {
  using value_type
    = std::chrono::time_point<std::chrono::system_clock, Duration>;

  using default_impl = default_inspector_access<value_type>;

  template <class Inspector>
  static bool apply_object(Inspector& f, value_type& x) {
    return f.object(x).fields(f.field("value", x));
  }

  template <class Inspector>
  static bool apply_value(Inspector& f, value_type& x) {
    if (f.has_human_readable_format()) {
      auto get = [&x] {
        std::string str;
        detail::print(str, x);
        return str;
      };
      auto set = [&x](std::string str) {
        auto err = detail::parse(str, x);
        return !err;
      };
      return detail::split_save_load(f, get, set);
    }
    using rep_type = typename Duration::rep;
    auto get = [&x] { return x.time_since_epoch().count(); };
    auto set = [&x](rep_type value) {
      x = value_type{Duration{value}};
      return true;
    };
    return detail::split_save_load(f, get, set);
  }
};

} // namespace caf
