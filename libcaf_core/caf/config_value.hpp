/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "caf/atom.hpp"
#include "caf/detail/bounds_checker.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/dictionary.hpp"
#include "caf/fwd.hpp"
#include "caf/optional.hpp"
#include "caf/raise_error.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/sum_type.hpp"
#include "caf/sum_type_access.hpp"
#include "caf/sum_type_token.hpp"
#include "caf/timestamp.hpp"
#include "caf/uri.hpp"
#include "caf/variant.hpp"

namespace caf {

/// A type for config parameters with similar interface to a `variant`. This
/// type is not implemented as a simple variant alias because variants cannot
/// contain lists of themselves.
class config_value {
public:
  // -- member types -----------------------------------------------------------

  using integer = int64_t;

  using boolean = bool;

  using real = double;

  using atom = atom_value;

  using timespan = caf::timespan;

  using string = std::string;

  using list = std::vector<config_value>;

  using dictionary = caf::dictionary<config_value>;

  using types = detail::type_list<integer, boolean, real, atom, timespan, uri,
                                  string, list, dictionary>;

  using variant_type = detail::tl_apply_t<types, variant>;

  // -- constructors, destructors, and assignment operators --------------------

  config_value() = default;

  config_value(config_value&& other) = default;

  config_value(const config_value& other) = default;

  template <class T, class E = detail::enable_if_t<
                       !std::is_same<detail::decay_t<T>, config_value>::value>>
  explicit config_value(T&& x) {
    set(std::forward<T>(x));
  }

  config_value& operator=(config_value&& other) = default;

  config_value& operator=(const config_value& other) = default;

  template <class T, class E = detail::enable_if_t<
                       !std::is_same<detail::decay_t<T>, config_value>::value>>
  config_value& operator=(T&& x) {
    set(std::forward<T>(x));
    return *this;
  }

  ~config_value();

  // -- parsing ----------------------------------------------------------------

  /// Tries to parse a value from `str`.
  static expected<config_value> parse(string_view::iterator first,
                                      string_view::iterator last);

  /// Tries to parse a value from `str`.
  static expected<config_value> parse(string_view str);

  // -- properties -------------------------------------------------------------

  /// Converts the value to a list with one element. Does nothing if the value
  /// already is a list.
  void convert_to_list();

  /// Appends `x` to a list. Converts this config value to a list first by
  /// calling `convert_to_list` if needed.
  void append(config_value x);

  /// Returns a human-readable type name of the current value.
  const char* type_name() const noexcept;

  /// Returns the underlying variant.
  inline variant_type& get_data() {
    return data_;
  }

  /// Returns the underlying variant.
  inline const variant_type& get_data() const {
    return data_;
  }

  /// Returns a pointer to the underlying variant.
  inline variant_type* get_data_ptr() {
    return &data_;
  }

  /// Returns a pointer to the underlying variant.
  inline const variant_type* get_data_ptr() const {
    return &data_;
  }

private:
  // -- properties -------------------------------------------------------------

  static const char* type_name_at_index(size_t index) noexcept;

  // -- auto conversion of related types ---------------------------------------

  inline void set(bool x) {
    data_ = x;
  }

  inline void set(float x) {
    data_ = static_cast<double>(x);
  }

  inline void set(const char* x) {
    data_ = std::string{x};
  }

  template <class T>
  detail::enable_if_t<detail::is_one_of<T, real, atom, timespan, uri, string,
                                        list, dictionary>::value>
  set(T x) {
    data_ = std::move(x);
  }

  template <class T>
  detail::enable_if_t<std::is_integral<T>::value> set(T x) {
    data_ = static_cast<int64_t>(x);
  }

  // -- member variables -------------------------------------------------------

  variant_type data_;
};

// -- related type aliases -----------------------------------------------------

/// Organizes config values into categories.
/// @relates config_value
using config_value_map = dictionary<config_value::dictionary>;

// -- SumType-like access ------------------------------------------------------

/// @relates config_value
template <class T>
struct config_value_access;

/// Delegates to config_value_access for all specialized versions.
template <class T, bool IsIntegral = std::is_integral<T>::value>
struct select_config_value_access {
  using type = config_value_access<T>;
};

/// Catches all non-specialized integer types.
template <class T>
struct select_config_value_access<T, true> {
  struct type {
    static bool is(const config_value& x) {
      auto ptr = caf::get_if<typename config_value::integer>(x.get_data_ptr());
      return ptr != nullptr && detail::bounds_checker<T>::check(*ptr);
    }

    static optional<T> get_if(const config_value* x) {
      auto ptr = caf::get_if<typename config_value::integer>(x->get_data_ptr());
      if (ptr != nullptr && detail::bounds_checker<T>::check(*ptr))
        return static_cast<T>(*ptr);
      return none;
    }

    static T get(const config_value& x) {
      auto res = get_if(&x);
      CAF_ASSERT(res != none);
      return *res;
    }
  };
};

template <class T>
using select_config_value_access_t =
  typename select_config_value_access<T>::type;

template <>
struct sum_type_access<config_value> {
  using types = typename config_value::types;

  using type0 = typename detail::tl_head<types>::type;

  static constexpr bool specialized = true;

  template <class U, int Pos>
  static bool is(const config_value& x, sum_type_token<U, Pos> token) {
    return x.get_data().is(token.pos);
  }

  template <class U>
  static bool is(const config_value& x, sum_type_token<U, -1>) {
    return select_config_value_access_t<U>::is(x);
  }

  template <class U, int Pos>
  static U& get(config_value& x, sum_type_token<U, Pos> token) {
    return x.get_data().get(token.pos);
  }

  template <class U>
  static U get(config_value& x, sum_type_token<U, -1>) {
    return select_config_value_access_t<U>::get(x);
  }

  template <class U, int Pos>
  static const U& get(const config_value& x, sum_type_token<U, Pos> token) {
    return x.get_data().get(token.pos);
  }

  template <class U>
  static U get(const config_value& x, sum_type_token<U, -1>) {
    return select_config_value_access_t<U>::get(x);
  }

  template <class U, int Pos>
  static U* get_if(config_value* x, sum_type_token<U, Pos> token) {
    return is(*x, token) ? &get(*x, token) : nullptr;
  }

  template <class U>
  static optional<U> get_if(config_value* x, sum_type_token<U, -1>) {
    return select_config_value_access_t<U>::get_if(x);
  }

  template <class U, int Pos>
  static const U* get_if(const config_value* x, sum_type_token<U, Pos> token) {
    return is(*x, token) ? &get(*x, token) : nullptr;
  }

  template <class U>
  static optional<U> get_if(const config_value* x, sum_type_token<U, -1>) {
    return select_config_value_access_t<U>::get_if(x);
  }

  template <class Result, class Visitor, class... Ts>
  static Result apply(config_value& x, Visitor&& visitor, Ts&&... xs) {
    return x.get_data().template apply<Result>(std::forward<Visitor>(visitor),
                                               std::forward<Ts>(xs)...);
  }

  template <class Result, class Visitor, class... Ts>
  static Result apply(const config_value& x, Visitor&& visitor, Ts&&... xs) {
    return x.get_data().template apply<Result>(std::forward<Visitor>(visitor),
                                               std::forward<Ts>(xs)...);
  }
};

template <>
struct config_value_access<float> {
  static constexpr bool specialized = true;

  static bool is(const config_value& x) {
    return holds_alternative<double>(x.get_data());
  }

  static optional<float> get_if(const config_value* x) {
    auto res = caf::get_if<double>(&(x->get_data()));
    if (res)
      return static_cast<float>(*res);
    return none;
  }

  static float get(const config_value& x) {
    return static_cast<float>(caf::get<double>(x.get_data()));
  }
};

/// Implements automagic unboxing of `std::vector<T>` from a homogeneous
/// `config_value::list`.
/// @relates config_value
template <class T>
struct config_value_access<std::vector<T>> {
  using vector_type = std::vector<T>;

  static constexpr bool specialized = true;

  static bool is(const config_value& x) {
    auto lst = caf::get_if<config_value::list>(&x);
    if (lst != nullptr) {
      return std::all_of(lst->begin(), lst->end(), [](const config_value& y) {
        return caf::holds_alternative<T>(y);
      });
    }
    return false;
  }

  static optional<vector_type> get_if(const config_value* x) {
    vector_type result;
    auto extract = [&](const config_value& y) {
      auto opt = caf::get_if<T>(&y);
      if (opt) {
        result.emplace_back(*opt);
        return true;
      }
      return false;
    };
    auto lst = caf::get_if<config_value::list>(x);
    if (lst != nullptr && std::all_of(lst->begin(), lst->end(), extract))
      return result;
    return none;
  }

  static vector_type get(const config_value& x) {
    auto result = get_if(&x);
    if (!result)
      CAF_RAISE_ERROR("invalid type found");
    return std::move(*result);
  }
};

/// Implements automagic unboxing of `dictionary<V>` from a homogeneous
/// `config_value::dictionary`.
/// @relates config_value
template <class V>
struct config_value_access<dictionary<V>> {
  using map_type = dictionary<V>;

  using kvp = std::pair<const std::string, config_value>;

  static constexpr bool specialized = true;

  static bool is(const config_value& x) {
    auto lst = caf::get_if<config_value::dictionary>(&x);
    if (lst != nullptr) {
      return std::all_of(lst->begin(), lst->end(), [](const kvp& y) {
        return holds_alternative<V>(y.second);
      });
    }
    return false;
  }

  static optional<map_type> get_if(const config_value* x) {
    map_type result;
    auto extract = [&](const kvp& y) {
      auto opt = caf::get_if<V>(&(y.second));
      if (opt) {
        result.emplace(y.first, std::move(*opt));
        return true;
      }
      return false;
    };
    auto lst = caf::get_if<config_value::dictionary>(x);
    if (lst != nullptr && std::all_of(lst->begin(), lst->end(), extract))
      return result;
    return none;
  }

  static map_type get(const config_value& x) {
    auto result = get_if(&x);
    if (!result)
      CAF_RAISE_ERROR("invalid type found");
    return std::move(*result);
  }
};

// -- SumType-like access of dictionary values ---------------------------------

/// Tries to retrieve the value associated to `name` from `xs`.
/// @relates config_value
template <class T>
optional<T> get_if(const config_value::dictionary* xs, string_view name) {
  // Split the name into a path.
  std::vector<string_view> path;
  split(path, name, ".");
  // Sanity check.
  if (path.size() == 0)
    return none;
  // Resolve path, i.e., find the requested submap.
  auto current = xs;
  auto leaf = path.end() - 1;
  for (auto i = path.begin(); i != leaf; ++i) {
    auto j = current->find(*i);
    if (j == current->end())
      return none;
    current = caf::get_if<config_value::dictionary>(&j->second);
    if (current == nullptr)
      return none;
  }
  // Get the leaf value.
  auto j = current->find(*leaf);
  if (j == current->end())
    return none;
  auto result = caf::get_if<T>(&j->second);
  if (result)
    return *result;
  return none;
}

/// Retrieves the value associated to `name` from `xs`.
/// @relates config_value
template <class T>
T get(const config_value::dictionary& xs, string_view name) {
  auto result = get_if<T>(&xs, name);
  if (result)
    return std::move(*result);
  CAF_RAISE_ERROR("invalid type or name found");
}

/// Retrieves the value associated to `name` from `xs` or returns
/// `default_value`.
/// @relates config_value
template <class T, class E = detail::enable_if_t<!std::is_pointer<T>::value>>
T get_or(const config_value::dictionary& xs, string_view name,
         const T& default_value) {
  auto result = get_if<T>(&xs, name);
  if (result)
    return std::move(*result);
  return default_value;
}

/// Retrieves the value associated to `name` from `xs` or returns
/// `default_value`.
/// @relates config_value
std::string get_or(const config_value::dictionary& xs, string_view name,
                   const char* default_value);

/// Tries to retrieve the value associated to `name` from `xs`.
/// @relates config_value
template <class T>
optional<T> get_if(const config_value_map* xs, string_view name) {
  // Get the category.
  auto pos = name.find('.');
  if (pos == std::string::npos) {
    auto i = xs->find("global");
    if (i == xs->end())
      return none;
    return get_if<T>(&i->second, name);
  }
  auto i = xs->find(name.substr(0, pos));
  if (i == xs->end())
    return none;
  return get_if<T>(&i->second, name.substr(pos + 1));
}

/// Retrieves the value associated to `name` from `xs`.
/// @relates config_value
template <class T>
T get(const config_value_map& xs, string_view name) {
  auto result = get_if<T>(&xs, name);
  if (result)
    return std::move(*result);
  CAF_RAISE_ERROR("invalid type or name found");
}

/// Retrieves the value associated to `name` from `xs` or returns
/// `default_value`.
/// @relates config_value
template <class T, class E = detail::enable_if_t<!std::is_pointer<T>::value>>
T get_or(const config_value_map& xs, string_view name, const T& default_value) {
  auto result = get_if<T>(&xs, name);
  if (result)
    return std::move(*result);
  return default_value;
}

/// Retrieves the value associated to `name` from `xs` or returns
/// `default_value`.
/// @relates config_value
std::string get_or(const config_value_map& xs, string_view name,
                   const char* default_value);

/// Tries to retrieve the value associated to `name` from `cfg`.
/// @relates config_value
template <class T>
optional<T> get_if(const actor_system_config* cfg, string_view name) {
  return get_if<T>(&content(*cfg), name);
}

/// Retrieves the value associated to `name` from `cfg`.
/// @relates config_value
template <class T>
T get(const actor_system_config& cfg, string_view name) {
  return get<T>(content(cfg), name);
}

/// Retrieves the value associated to `name` from `cfg` or returns
/// `default_value`.
/// @relates config_value
template <class T, class E = detail::enable_if_t<!std::is_pointer<T>::value>>
T get_or(const actor_system_config& cfg, string_view name,
         const T& default_value) {
  return get_or(content(cfg), name, default_value);
}

std::string get_or(const actor_system_config& cfg, string_view name,
                   const char* default_value);

/// @private
void put_impl(config_value::dictionary& dict,
              const std::vector<string_view>& path, config_value& value);

/// @private
void put_impl(config_value::dictionary& dict, string_view key,
              config_value& value);

/// @private
void put_impl(dictionary<config_value::dictionary>& dict, string_view key,
              config_value& value);

/// Converts `value` to a `config_value` and assigns it to `key`.
/// @param dict Dictionary of key-value pairs.
/// @param key Human-readable nested keys in the form `category.key`.
/// @param value New value for given `key`.
template <class T>
void put(dictionary<config_value::dictionary>& dict, string_view key,
              T&& value) {
  config_value tmp{std::forward<T>(value)};
  put_impl(dict, key, tmp);
}


/// @relates config_value
bool operator<(const config_value& x, const config_value& y);

/// @relates config_value
bool operator==(const config_value& x, const config_value& y);

/// @relates config_value
inline bool operator>=(const config_value& x, const config_value& y) {
  return !(x < y);
}

/// @relates config_value
inline bool operator!=(const config_value& x, const config_value& y) {
  return !(x == y);
}

/// @relates config_value
std::string to_string(const config_value& x);

template <class... Ts>
config_value make_config_value_list(Ts&&... xs) {
  std::vector<config_value> lst{config_value{std::forward<Ts>(xs)}...};
  return config_value{std::move(lst)};
}

/// @relates config_value
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, config_value& x) {
  return f(meta::type_name("config_value"), x.get_data());
}

} // namespace caf

