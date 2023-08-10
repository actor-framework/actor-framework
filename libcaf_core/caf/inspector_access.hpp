// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/allowed_unsafe_message_type.hpp"
#include "caf/detail/as_mutable_ref.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/print.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector_access_base.hpp"
#include "caf/inspector_access_type.hpp"
#include "caf/intrusive_cow_ptr.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/optional.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string_view>
#include <tuple>
#include <utility>

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

// Converts a setter that returns void, error or bool to a sync function object
// taking no arguments that always returns a bool.
template <class Inspector, class Set, class ValueType>
auto bind_setter(Inspector& f, Set& set, ValueType& tmp) {
  using set_result_type = decltype(set(std::move(tmp)));
  if constexpr (std::is_same_v<set_result_type, bool>) {
    return [&] { return set(std::move(tmp)); };
  } else if constexpr (std::is_same_v<set_result_type, error>) {
    return [&] {
      if (auto err = set(std::move(tmp)); !err) {
        return true;
      } else {
        f.set_error(std::move(err));
        return false;
      }
    };
  } else {
    static_assert(std::is_same_v<set_result_type, void>,
                  "a setter must return caf::error, bool or void");
    return [&] {
      set(std::move(tmp));
      return true;
    };
  }
}

template <class Inspector, class T>
bool load(Inspector& f, T& x, inspector_access_type::specialization) {
  return inspector_access<T>::apply(f, x);
}

template <class Inspector, class T>
bool load(Inspector& f, T& x, inspector_access_type::inspect) {
  return inspect(f, x);
}

template <class Inspector, class T>
bool load(Inspector& f, T& x, inspector_access_type::builtin) {
  return f.value(x);
}

template <class Inspector, class T>
bool load(Inspector& f, T& x, inspector_access_type::builtin_inspect) {
  return f.builtin_inspect(x);
}

template <class Inspector, class T>
bool load(Inspector& f, T& x, inspector_access_type::empty) {
  return f.object(x).fields();
}

template <class Inspector, class T>
bool load(Inspector& f, T&, inspector_access_type::unsafe) {
  f.emplace_error(sec::unsafe_type);
  return false;
}

template <class Inspector, class T, size_t N>
bool load(Inspector& f, T (&xs)[N], inspector_access_type::tuple) {
  return f.tuple(xs);
}

template <class Inspector, class T>
bool load(Inspector& f, T& xs, inspector_access_type::tuple) {
  return f.tuple(xs);
}

template <class Inspector, class T>
bool load(Inspector& f, T& x, inspector_access_type::map) {
  return f.map(x);
}

template <class Inspector, class T>
bool load(Inspector& f, T& x, inspector_access_type::list) {
  return f.list(x);
}

template <class Inspector, class T>
std::enable_if_t<accepts_opaque_value_v<Inspector, T>, bool>
load(Inspector& f, T& x, inspector_access_type::none) {
  return f.opaque_value(x);
}

template <class Inspector, class T>
std::enable_if_t<!accepts_opaque_value_v<Inspector, T>, bool>
load(Inspector&, T&, inspector_access_type::none) {
  static_assert(
    detail::assertion_failed_v<T>,
    "please provide an inspect overload for T or specialize inspector_access");
  return false;
}

template <class Inspector, class T>
bool load(Inspector& f, T& x) {
  return load(f, x, inspect_access_type<Inspector, T>());
}

template <class Inspector, class T, class IsValid, class SyncValue>
bool load_field(Inspector& f, std::string_view field_name, T& x,
                IsValid& is_valid, SyncValue& sync_value) {
  using impl = std::conditional_t<is_complete<inspector_access<T>>, // if
                                  inspector_access<T>,              // then
                                  inspector_access_base<T>>;        // else
  return impl::load_field(f, field_name, x, is_valid, sync_value);
}

template <class Inspector, class T, class IsValid, class SyncValue,
          class SetFallback>
bool load_field(Inspector& f, std::string_view field_name, T& x,
                IsValid& is_valid, SyncValue& sync_value,
                SetFallback& set_fallback) {
  using impl = std::conditional_t<is_complete<inspector_access<T>>, // if
                                  inspector_access<T>,              // then
                                  inspector_access_base<T>>;        // else
  return impl::load_field(f, field_name, x, is_valid, sync_value, set_fallback);
}

// -- saving -------------------------------------------------------------------

template <class Inspector, class T>
bool save(Inspector& f, T& x, inspector_access_type::specialization) {
  return inspector_access<T>::apply(f, x);
}

template <class Inspector, class T>
bool save(Inspector& f, T& x, inspector_access_type::inspect) {
  return inspect(f, x);
}

template <class Inspector, class T>
bool save(Inspector& f, T& x, inspector_access_type::builtin) {
  return f.value(x);
}

template <class Inspector, class T>
bool save(Inspector& f, T& x, inspector_access_type::builtin_inspect) {
  return f.builtin_inspect(x);
}

template <class Inspector, class T>
bool save(Inspector& f, T& x, inspector_access_type::empty) {
  return f.object(x).fields();
}

template <class Inspector, class T>
bool save(Inspector& f, T&, inspector_access_type::unsafe) {
  f.emplace_error(sec::unsafe_type);
  return false;
}

template <class Inspector, class T, size_t N>
bool save(Inspector& f, T (&xs)[N], inspector_access_type::tuple) {
  return f.tuple(xs);
}

template <class Inspector, class T>
bool save(Inspector& f, const T& xs, inspector_access_type::tuple) {
  return f.tuple(xs);
}

template <class Inspector, class T>
bool save(Inspector& f, T& x, inspector_access_type::map) {
  return f.map(x);
}

template <class Inspector, class T>
bool save(Inspector& f, T& x, inspector_access_type::list) {
  return f.list(x);
}

template <class Inspector, class T>
std::enable_if_t<accepts_opaque_value_v<Inspector, T>, bool>
save(Inspector& f, T& x, inspector_access_type::none) {
  return f.opaque_value(x);
}

template <class Inspector, class T>
std::enable_if_t<!accepts_opaque_value_v<Inspector, T>, bool>
save(Inspector&, T&, inspector_access_type::none) {
  static_assert(
    detail::assertion_failed_v<T>,
    "please provide an inspect overload for T or specialize inspector_access");
  return false;
}

template <class Inspector, class T>
bool save(Inspector& f, T& x) {
  return save(f, x, inspect_access_type<Inspector, T>());
}

template <class Inspector, class T>
bool save(Inspector& f, const T& x) {
  if constexpr (!std::is_function_v<T>) {
    return save(f, as_mutable_ref(x), inspect_access_type<Inspector, T>());
  } else {
    // Only inspector such as the stringification_inspector are going to accept
    // function pointers. Most other inspectors are going to trigger a static
    // assertion when passing `inspector_access_type::none`.
    auto fptr = std::add_pointer_t<T>{x};
    return save(f, fptr, inspector_access_type::none{});
  }
}

template <class Inspector, class T>
bool save_field(Inspector& f, std::string_view field_name, T& x) {
  using impl = std::conditional_t<is_complete<inspector_access<T>>, // if
                                  inspector_access<T>,              // then
                                  inspector_access_base<T>>;        // else
  return impl::save_field(f, field_name, x);
}

template <class Inspector, class IsPresent, class Get>
bool save_field(Inspector& f, std::string_view field_name,
                IsPresent& is_present, Get& get) {
  using T = std::decay_t<decltype(get())>;
  using impl = std::conditional_t<is_complete<inspector_access<T>>, // if
                                  inspector_access<T>,              // then
                                  inspector_access_base<T>>;        // else
  return impl::save_field(f, field_name, is_present, get);
}

} // namespace caf::detail

namespace caf {

// -- customization points -----------------------------------------------------

/// Customization point for adding support for a custom type.
template <class T>
struct inspector_access;

// -- inspection support for optional values -----------------------------------

struct optional_inspector_traits_base {
  template <class T>
  static auto& deref_load(T& x) {
    return *x;
  }

  template <class T>
  static auto& deref_save(T& x) {
    return *x;
  }
};

template <class T>
struct optional_inspector_traits;

CAF_PUSH_DEPRECATED_WARNING

template <class T>
struct optional_inspector_traits<optional<T>> : optional_inspector_traits_base {
  using container_type = optional<T>;

  using value_type = T;

  template <class... Ts>
  static void emplace(container_type& container, Ts&&... xs) {
    container.emplace(std::forward<Ts>(xs)...);
  }
};

CAF_POP_WARNINGS

template <class T>
struct optional_inspector_traits<intrusive_ptr<T>>
  : optional_inspector_traits_base {
  using container_type = intrusive_ptr<T>;

  using value_type = T;

  template <class... Ts>
  static void emplace(container_type& container, Ts&&... xs) {
    container.reset(new T(std::forward<Ts>(xs)...));
  }
};

template <class T>
struct optional_inspector_traits<intrusive_cow_ptr<T>> {
  using container_type = intrusive_cow_ptr<T>;

  using value_type = T;

  template <class... Ts>
  static void emplace(container_type& container, Ts&&... xs) {
    container.reset(new T(std::forward<Ts>(xs)...));
  }

  static value_type& deref_load(container_type& x) {
    return x.unshared();
  }

  static const value_type& deref_save(container_type& x) {
    return *x;
  }
};

template <class T>
struct optional_inspector_traits<std::unique_ptr<T>>
  : optional_inspector_traits_base {
  using container_type = std::unique_ptr<T>;

  using value_type = T;

  template <class... Ts>
  static void emplace(container_type& container, Ts&&... xs) {
    container = std::make_unique<T>(std::forward<Ts>(xs)...);
  }
};

template <class T>
struct optional_inspector_traits<std::shared_ptr<T>>
  : optional_inspector_traits_base {
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
  [[nodiscard]] static bool apply(Inspector& f, container_type& x) {
    return f.object(x).fields(f.field("value", x));
  }

  template <class Inspector>
  static bool
  save_field(Inspector& f, std::string_view field_name, container_type& x) {
    auto is_present = [&x] { return static_cast<bool>(x); };
    auto get = [&x]() -> decltype(auto) { return traits::deref_save(x); };
    return detail::save_field(f, field_name, is_present, get);
  }

  template <class Inspector, class IsPresent, class Get>
  static bool save_field(Inspector& f, std::string_view field_name,
                         IsPresent& is_present, Get& get) {
    auto deref_get = [&get]() -> decltype(auto) {
      return traits::deref_save(get());
    };
    return detail::save_field(f, field_name, is_present, deref_get);
  }

  template <class Inspector, class IsValid, class SyncValue>
  static bool load_field(Inspector& f, std::string_view field_name,
                         container_type& x, IsValid& is_valid,
                         SyncValue& sync_value) {
    traits::emplace(x);
    auto reset = [&x] { x.reset(); };
    return detail::load_field(f, field_name, traits::deref_load(x), is_valid,
                              sync_value, reset);
  }

  template <class Inspector, class IsValid, class SyncValue, class SetFallback>
  static bool load_field(Inspector& f, std::string_view field_name,
                         container_type& x, IsValid& is_valid,
                         SyncValue& sync_value, SetFallback& set_fallback) {
    traits::emplace(x);
    return detail::load_field(f, field_name, traits::deref_load(x), is_valid,
                              sync_value, set_fallback);
  }
};

// -- inspection support for optional<T> ---------------------------------------

CAF_PUSH_DEPRECATED_WARNING

template <class T>
struct inspector_access<optional<T>> : optional_inspector_access<optional<T>> {
  // nop
};

CAF_POP_WARNINGS

template <class T>
struct optional_inspector_traits<std::optional<T>>
  : optional_inspector_traits_base {
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

// -- inspection support for error ---------------------------------------------

template <>
struct inspector_access<std::unique_ptr<error::data>>
  : optional_inspector_access<std::unique_ptr<error::data>> {
  // nop
};

// -- inspection support for std::byte -----------------------------------------

template <>
struct inspector_access<std::byte> : inspector_access_base<std::byte> {
  template <class Inspector>
  [[nodiscard]] static bool apply(Inspector& f, std::byte& x) {
    using integer_type = std::underlying_type_t<std::byte>;
    auto get = [&x] { return static_cast<integer_type>(x); };
    auto set = [&x](integer_type val) { x = static_cast<std::byte>(val); };
    return f.apply(get, set);
  }
};

// -- inspection support for variant<Ts...> ------------------------------------

template <class T>
struct variant_inspector_traits;

template <class T>
struct variant_inspector_access {
  using value_type = T;

  using traits = variant_inspector_traits<T>;

  template <class Inspector>
  [[nodiscard]] static bool apply(Inspector& f, value_type& x) {
    return f.object(x).fields(f.field("value", x));
  }

  template <class Inspector>
  static bool
  save_field(Inspector& f, std::string_view field_name, value_type& x) {
    auto g = [&f](auto& y) { return detail::save(f, y); };
    return f.begin_field(field_name, make_span(traits::allowed_types),
                         traits::type_index(x)) //
           && traits::visit(g, x)               //
           && f.end_field();
  }

  template <class Inspector, class IsPresent, class Get>
  static bool save_field(Inspector& f, std::string_view field_name,
                         IsPresent& is_present, Get& get) {
    auto allowed_types = make_span(traits::allowed_types);
    if (is_present()) {
      auto&& x = get();
      auto g = [&f](auto& y) { return detail::save(f, y); };
      return f.begin_field(field_name, true, allowed_types,
                           traits::type_index(x)) //
             && traits::visit(g, x)               //
             && f.end_field();
    }
    return f.begin_field(field_name, false, allowed_types, 0) //
           && f.end_field();
  }

  template <class Inspector>
  static bool load_variant_value(Inspector& f, std::string_view field_name,
                                 value_type& x, type_id_t runtime_type) {
    auto res = false;
    auto type_found = traits::load(runtime_type, [&](auto& tmp) {
      if (!detail::load(f, tmp))
        return;
      traits::assign(x, std::move(tmp));
      res = true;
      return;
    });
    if (!type_found)
      f.emplace_error(sec::invalid_field_type, std::string{field_name});
    return res;
  }

  template <class Inspector, class IsValid, class SyncValue>
  static bool load_field(Inspector& f, std::string_view field_name,
                         value_type& x, IsValid& is_valid,
                         SyncValue& sync_value) {
    size_t type_index = std::numeric_limits<size_t>::max();
    auto allowed_types = make_span(traits::allowed_types);
    if (!f.begin_field(field_name, allowed_types, type_index))
      return false;
    if (type_index >= allowed_types.size()) {
      f.emplace_error(sec::invalid_field_type, std::string{field_name});
      return false;
    }
    auto runtime_type = allowed_types[type_index];
    if (!load_variant_value(f, field_name, x, runtime_type))
      return false;
    if (!is_valid(x)) {
      f.emplace_error(sec::field_invariant_check_failed,
                      std::string{field_name});
      return false;
    }
    if (!sync_value()) {
      if (!f.get_error())
        f.emplace_error(sec::field_value_synchronization_failed,
                        std::string{field_name});
      return false;
    }
    return f.end_field();
  }

  template <class Inspector, class IsValid, class SyncValue, class SetFallback>
  static bool load_field(Inspector& f, std::string_view field_name,
                         value_type& x, IsValid& is_valid,
                         SyncValue& sync_value, SetFallback& set_fallback) {
    bool is_present = false;
    auto allowed_types = make_span(traits::allowed_types);
    size_t type_index = std::numeric_limits<size_t>::max();
    if (!f.begin_field(field_name, is_present, allowed_types, type_index))
      return false;
    if (is_present) {
      if (type_index >= allowed_types.size()) {
        f.emplace_error(sec::invalid_field_type, std::string{field_name});
        return false;
      }
      auto runtime_type = allowed_types[type_index];
      if (!load_variant_value(f, field_name, x, runtime_type))
        return false;
      if (!is_valid(x)) {
        f.emplace_error(sec::field_invariant_check_failed,
                        std::string{field_name});
        return false;
      }
      if (!sync_value()) {
        if (!f.get_error())
          f.emplace_error(sec::field_value_synchronization_failed,
                          std::string{field_name});
        return false;
      }
    } else {
      set_fallback();
    }
    return f.end_field();
  }
};

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

// -- inspection support for std::chrono types ---------------------------------

template <class Rep, class Period>
struct inspector_access<std::chrono::duration<Rep, Period>>
  : inspector_access_base<std::chrono::duration<Rep, Period>> {
  using value_type = std::chrono::duration<Rep, Period>;

  template <class Inspector>
  static bool apply(Inspector& f, value_type& x) {
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
      return f.apply(get, set);
    } else {
      auto get = [&x] { return x.count(); };
      auto set = [&x](Rep value) {
        x = std::chrono::duration<Rep, Period>{value};
        return true;
      };
      return f.apply(get, set);
    }
  }
};

template <class Duration>
struct inspector_access<
  std::chrono::time_point<std::chrono::system_clock, Duration>>
  : inspector_access_base<
      std::chrono::time_point<std::chrono::system_clock, Duration>> {
  using value_type
    = std::chrono::time_point<std::chrono::system_clock, Duration>;

  template <class Inspector>
  static bool apply(Inspector& f, value_type& x) {
    if (f.has_human_readable_format()) {
      auto get = [&x] {
        std::string str;
        detail::print(str, x);
        return str;
      };
      auto set = [&x](std::string str) { return detail::parse(str, x); };
      return f.apply(get, set);
    } else {
      using rep_type = typename Duration::rep;
      auto get = [&x] { return x.time_since_epoch().count(); };
      auto set = [&x](rep_type value) {
        x = value_type{Duration{value}};
        return true;
      };
      return f.apply(get, set);
    }
  }
};

} // namespace caf
