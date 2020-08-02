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

#include <tuple>

#include "caf/detail/inspect.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/optional.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/sum_type_access.hpp"
#include "caf/variant.hpp"

namespace caf {

// -- utility function objects and functions for load and save inspectors ------

namespace detail {

/// Utility class for predicates that always return `true`.
struct always_true_t {
  template <class... Ts>
  [[nodiscard]] constexpr bool operator()(Ts&&...) const noexcept {
    return true;
  }
};

/// Predicate that always returns `true`.
constexpr auto always_true = always_true_t{};

/// Returns a mutable reference to `x`.
template <class T>
T& as_mutable_ref(T& x) {
  return x;
}

/// Returns a mutable reference to `x`.
template <class T>
T& as_mutable_ref(const T& x) {
  return const_cast<T&>(x);
}

/// Checks whether the inspector has a `value` overload for `T`.
template <class Inspector, class T>
class is_trivially_inspectable {
private:
  template <class U>
  static auto sfinae(Inspector& f, U& x)
    -> decltype(f.value(x), std::true_type{});

  static std::false_type sfinae(Inspector&, ...);

  using sfinae_result
    = decltype(sfinae(std::declval<Inspector&>(), std::declval<T&>()));

public:
  static constexpr bool value = sfinae_result::value;
};

} // namespace detail

/// Calls `f.value(x)` if this is a valid expression,
/// `inspector_access<T>::apply_value(f, x)` otherwise.
template <class Inspector, class T>
bool inspect_value(Inspector& f, T& x) {
  if constexpr (detail::is_trivially_inspectable<Inspector, T>::value) {
    return f.value(x);
  } else {
    return inspector_access<T>::apply_value(f, x);
  }
}

/// @copydoc inspect_value
template <class Inspector, class T>
bool inspect_value(Inspector& f, const T& x) {
  if constexpr (detail::is_trivially_inspectable<Inspector, T>::value) {
    return f.value(const_cast<T&>(x));
  } else {
    return inspector_access<T>::apply_value(f, const_cast<T&>(x));
  }
}

template <class Inspector, class Get, class Set>
bool inspect_value(Inspector& f, Get&& get, Set&& set) {
  using value_type = std::decay_t<decltype(get())>;
  if constexpr (Inspector::is_loading) {
    auto tmp = value_type{};
    if (inspect_value(f, tmp))
      return set(std::move(tmp));
    return false;
  } else {
    auto&& x = get();
    return inspect_value(f, detail::as_mutable_ref(x));
  }
}

/// Provides default implementations for `save_field`, `load_field`, and
/// `apply_value`.
template <class T>
struct inspector_access_base {
  /// Saves a mandatory field to `f`.
  template <class Inspector>
  static bool save_field(Inspector& f, string_view field_name, T& x) {
    return f.begin_field(field_name) && inspect_value(f, x) && f.end_field();
  }

  /// Saves an optional field to `f`.
  template <class Inspector, class IsPresent, class Get>
  static bool save_field(Inspector& f, string_view field_name,
                         IsPresent& is_present, Get& get) {
    if (is_present()) {
      auto&& x = get();
      return f.begin_field(field_name, true) //
             && inspect_value(f, x)          //
             && f.end_field();
    }
    return f.begin_field(field_name, false) && f.end_field();
  }

  /// Loads a mandatory field from `f`.
  template <class Inspector, class IsValid, class SyncValue>
  static bool load_field(Inspector& f, string_view field_name, T& x,
                         IsValid& is_valid, SyncValue& sync_value) {
    if (f.begin_field(field_name) && inspect_value(f, x)) {
      if (!is_valid(x))
        return f.load_field_failed(field_name,
                                   sec::field_invariant_check_failed);
      if (!sync_value())
        return f.load_field_failed(field_name,
                                   sec::field_value_synchronization_failed);
      return f.end_field();
    }
    return false;
  }

  /// Loads an optional field from `f`, calling `set_fallback` if the source
  /// contains no value for `x`.
  template <class Inspector, class IsValid, class SyncValue, class SetFallback>
  static bool load_field(Inspector& f, string_view field_name, T& x,
                         IsValid& is_valid, SyncValue& sync_value,
                         SetFallback& set_fallback) {
    bool is_present = false;
    if (!f.begin_field(field_name, is_present))
      return false;
    if (is_present) {
      if (!inspect_value(f, x))
        return false;
      if (!is_valid(x))
        return f.load_field_failed(field_name,
                                   sec::field_invariant_check_failed);
      if (!sync_value())
        return f.load_field_failed(field_name,
                                   sec::field_value_synchronization_failed);
      return f.end_field();
    }
    set_fallback();
    return f.end_field();
  }
};

/// Default implementation for @ref inspector_access.
template <class T>
struct default_inspector_access : inspector_access_base<T> {
  /// Applies `x` as an object to `f`.
  template <class Inspector>
  [[nodiscard]] static bool apply_object(Inspector& f, T& x) {
    using detail::inspect;
    // User-provided `inspect` overloads come first.
    if constexpr (detail::is_inspectable<Inspector, T>::value) {
      using result_type = decltype(inspect(f, x));
      if constexpr (std::is_same<result_type, bool>::value)
        return inspect(f, x);
      else
        return apply_deprecated(f, x);
    } else if constexpr (Inspector::is_loading) {
      return load_object(f, x);
    } else {
      return save_object(f, x);
    }
  }

  /// Applies `x` as a single value to `f`.
  template <class Inspector>
  [[nodiscard]] static bool apply_value(Inspector& f, T& x) {
    return apply_object(f, x);
  }

  template <class Inspector>
  [[nodiscard]] static bool load_object(Inspector& f, T& x) {
    if constexpr (std::is_arithmetic<T>::value) {
      return f.object(x).fields(f.field("value", x));
    }
  }

  template <class Inspector>
  [[deprecated("inspect() overloads should return bool")]] //
  [[nodiscard]] static bool
  apply_deprecated(Inspector& f, T& x) {
    if (auto err = inspect(f, x)) {
      f.set_error(std::move(err));
      return false;
    }
    return true;
  }
};

/// Customization point for adding support for a custom type. The default
/// implementation requires an `inspect` overload for `T` that is available via
/// ADL.
template <class T>
struct inspector_access : default_inspector_access<T> {};

template <class... Ts>
struct inspector_access<std::tuple<Ts...>> {
  // -- boilerplate ------------------------------------------------------------

  using value_type = std::tuple<Ts...>;

  using tuple_indexes = std::make_index_sequence<sizeof...(Ts)>;

  template <class Inspector>
  static bool apply_object(Inspector& f, value_type& x) {
    return f.object(x).fields(f.field("value", x));
  }

  template <class Inspector>
  static bool apply_value(Inspector& f, value_type& x) {
    return apply_object(f, x);
  }

  // -- saving -----------------------------------------------------------------

  template <class Inspector, class IsPresent, class Get, size_t... Is>
  static bool save_field(Inspector& f, string_view field_name,
                         IsPresent&& is_present, Get&& get,
                         std::index_sequence<Is...>) {
    if (is_present()) {
      auto&& x = get();
      return f.begin_field(field_name, true)               //
             && f.begin_tuple(sizeof...(Ts))               //
             && (inspect_value(f, std::get<Is>(x)) && ...) //
             && f.end_tuple()                              //
             && f.end_field();
    } else {
      return f.begin_field(field_name, false) && f.end_field();
    }
  }

  template <class Inspector, class IsPresent, class Get>
  static bool save_field(Inspector& f, string_view field_name,
                         IsPresent&& is_present, Get&& get) {
    return save_field(f, field_name, std::forward<IsPresent>(is_present),
                      std::forward<Get>(get), tuple_indexes{});
  }

  template <class Inspector, size_t... Is>
  static bool save_field(Inspector& f, string_view field_name, value_type& x,
                         std::index_sequence<Is...>) {
    return f.begin_field(field_name)                     //
           && f.begin_tuple(sizeof...(Ts))               //
           && (inspect_value(f, std::get<Is>(x)) && ...) //
           && f.end_tuple()                              //
           && f.end_field();
  }

  template <class Inspector>
  static bool save_field(Inspector& f, string_view field_name, value_type& x) {
    return save_field(f, field_name, x, tuple_indexes{});
  }

  // -- loading ----------------------------------------------------------------

  template <class Inspector, class IsValid, class SyncValue, size_t... Is>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value,
                         std::index_sequence<Is...>) {
    if (f.begin_field(field_name)       //
        && f.begin_tuple(sizeof...(Ts)) //
        && (inspect_value(f, std::get<Is>(x)) && ...)) {
      if (!is_valid(x))
        return f.load_field_failed(field_name,
                                   sec::field_invariant_check_failed);
      if (!sync_value())
        return f.load_field_failed(field_name,
                                   sec::field_value_synchronization_failed);
      return f.end_tuple() && f.end_field();
    }
    return false;
  }

  template <class Inspector, class IsValid, class SyncValue>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value) {
    return load_field(f, field_name, x, is_valid, sync_value, tuple_indexes{});
  }

  template <class Inspector, class IsValid, class SyncValue, class SetFallback,
            size_t... Is>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value,
                         SetFallback& set_fallback,
                         std::index_sequence<Is...>) {
    bool is_present = false;
    if (!f.begin_field(field_name, is_present))
      return false;
    if (is_present) {
      if (!f.begin_tuple(sizeof...(Ts)) //
          || !(inspect_value(f, std::get<Is>(x)) && ...))
        return false;
      if (!is_valid(x))
        return f.load_field_failed(field_name,
                                   sec::field_invariant_check_failed);
      if (!sync_value())
        return f.load_field_failed(field_name,
                                   sec::field_value_synchronization_failed);
      return f.end_tuple() && f.end_field();
    }
    set_fallback();
    return f.end_field();
  }

  template <class Inspector, class IsValid, class SyncValue, class SetFallback>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value,
                         SetFallback& set_fallback) {
    return load_field(f, field_name, x, is_valid, sync_value, set_fallback,
                      tuple_indexes{});
  }
};

template <class T>
struct inspector_access<optional<T>> {
  using value_type = optional<T>;

  template <class Inspector>
  [[nodiscard]] static bool apply_object(Inspector& f, value_type& x) {
    return f.object(x).fields(f.field("value", x));
  }

  template <class Inspector>
  [[nodiscard]] static bool apply_value(Inspector& f, value_type&) {
    f.set_error(
      make_error(sec::runtime_error, "apply_value called on an optional"));
    return false;
  }

  template <class Inspector>
  static bool save_field(Inspector& f, string_view field_name, value_type& x) {
    auto is_present = [&x] { return static_cast<bool>(x); };
    auto get = [&x] { return *x; };
    return inspector_access<T>::save_field(f, field_name, is_present, get);
  }

  template <class Inspector, class IsPresent, class Get>
  static bool save_field(Inspector& f, string_view field_name,
                         IsPresent& is_present, Get& get) {
    if (is_present()) {
      auto&& x = get();
      return save_field(f, field_name, detail::as_mutable_ref(x));
    }
    return f.begin_field(field_name, false) && f.end_field();
  }

  template <class Inspector, class IsValid, class SyncValue>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value) {
    auto tmp = T{};
    auto set_x = [&] {
      x = std::move(tmp);
      return sync_value();
    };
    auto reset = [&x] { x = value_type{}; };
    return inspector_access<T>::load_field(f, field_name, tmp, is_valid, set_x,
                                           reset);
  }

  template <class Inspector, class IsValid, class SyncValue, class SetFallback>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value,
                         SetFallback& set_fallback) {
    auto tmp = T{};
    auto set_x = [&] {
      x = std::move(tmp);
      return sync_value();
    };
    return inspector_access<T>::load_field(f, field_name, tmp, is_valid, set_x,
                                           set_fallback);
  }
};

template <class... Ts>
struct inspector_access<variant<Ts...>> {
  using value_type = variant<Ts...>;

  static constexpr type_id_t allowed_types_arr[] = {type_id_v<Ts>...};

  template <class Inspector>
  [[nodiscard]] static bool apply_object(Inspector& f, value_type& x) {
    return f.object(x).fields(f.field("value", x));
  }

  template <class Inspector>
  [[nodiscard]] static bool apply_value(Inspector& f, value_type&) {
    f.set_error(
      make_error(sec::runtime_error, "apply_value called on a variant"));
    return false;
  }

  template <class Inspector>
  static bool save_field(Inspector& f, string_view field_name, value_type& x) {
    auto g = [&f](auto& y) { return inspect_value(f, y); };
    return f.begin_field(field_name, make_span(allowed_types_arr), x.index()) //
           && visit(g, x)                                                     //
           && f.end_field();
  }

  template <class Inspector, class IsPresent, class Get>
  static bool save_field(Inspector& f, string_view field_name,
                         IsPresent& is_present, Get& get) {
    auto allowed_types = make_span(allowed_types_arr);
    if (is_present()) {
      auto&& x = get();
      auto g = [&f](auto& y) { return inspect_value(f, y); };
      return f.begin_field(field_name, true, allowed_types, x.index()) //
             && visit(g, x)                                            //
             && f.end_field();
    }
    return f.begin_field(field_name, false, allowed_types, 0) //
           && f.end_field();
  }

  template <class Inspector>
  static bool load_variant_value(Inspector& f, string_view field_name,
                                 value_type&, type_id_t, detail::type_list<>) {
    return f.load_field_failed(field_name, sec::invalid_field_type);
  }

  template <class Inspector, class U, class... Us>
  static bool load_variant_value(Inspector& f, string_view field_name,
                                 value_type& x, type_id_t runtime_type,
                                 detail::type_list<U, Us...>) {
    if (type_id_v<U> == runtime_type) {
      auto tmp = U{};
      if (!inspect_value(f, tmp))
        return false;
      x = std::move(tmp);
      return true;
    }
    detail::type_list<Us...> token;
    return load_variant_value(f, field_name, x, runtime_type, token);
  }

  template <class Inspector, class IsValid, class SyncValue>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value) {
    size_t type_index = std::numeric_limits<size_t>::max();
    auto allowed_types = make_span(allowed_types_arr);
    if (!f.begin_field(field_name, allowed_types, type_index))
      return false;
    if (type_index >= allowed_types.size())
      return f.load_field_failed(field_name, sec::invalid_field_type);
    auto runtime_type = allowed_types[type_index];
    detail::type_list<Ts...> types;
    if (!load_variant_value(f, field_name, x, runtime_type, types))
      return false;
    if (!is_valid(x))
      return f.load_field_failed(field_name, sec::field_invariant_check_failed);
    if (!sync_value())
      return f.load_field_failed(field_name,
                                 sec::field_value_synchronization_failed);
    return f.end_field();
  }

  template <class Inspector, class IsValid, class SyncValue, class SetFallback>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value,
                         SetFallback& set_fallback) {
    bool is_present = false;
    auto allowed_types = make_span(allowed_types_arr);
    size_t type_index = std::numeric_limits<size_t>::max();
    if (!f.begin_field(field_name, is_present, allowed_types, type_index))
      return false;
    if (is_present) {
      if (type_index >= allowed_types.size())
        return f.load_field_failed(field_name, sec::invalid_field_type);
      auto runtime_type = allowed_types[type_index];
      detail::type_list<Ts...> types;
      if (!load_variant_value(f, field_name, x, runtime_type, types))
        return false;
      if (!is_valid(x))
        return f.load_field_failed(field_name,
                                   sec::field_invariant_check_failed);
      if (!sync_value())
        return f.load_field_failed(field_name,
                                   sec::field_value_synchronization_failed);
    } else {
      set_fallback();
    }
    return f.end_field();
  }
};

} // namespace caf
