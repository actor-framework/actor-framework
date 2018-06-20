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
#include "caf/fwd.hpp"
#include "caf/optional.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/timestamp.hpp"
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

  using dictionary = std::map<std::string, config_value>;

  using types = detail::type_list<integer, boolean, real, atom, timespan,
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
  static expected<config_value> parse(std::string::const_iterator first,
                                      std::string::const_iterator last);

  /// Tries to parse a value from `str`.
  static expected<config_value> parse(const std::string& str);

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
  detail::enable_if_t<
    detail::is_one_of<T, real, atom, timespan, string, list, dictionary>::value>
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

/// Organizes config values into categories.
/// @relates config_value
using config_value_map = std::map<std::string, config_value::dictionary>;

/// @relates config_value
template <class T>
struct config_value_access {
  static constexpr bool specialized = false;
};

/// @relates config_value_access
template <class T>
struct default_config_value_access {
  static constexpr bool specialized = true;

  static bool is(const config_value& x) {
    return holds_alternative<T>(x.get_data());
  }

  static T* get_if(config_value* x) {
    return caf::get_if<T>(&(x->get_data()));
  }

  static const T* get_if(const config_value* x) {
    return caf::get_if<T>(&(x->get_data()));
  }

  static T& get(config_value& x) {
    return caf::get<T>(x.get_data());
  }
  static const T& get(const config_value& x) {
    return caf::get<T>(x.get_data());
  }
};

#define CAF_CONFIG_VALUE_DEFAULT_ACCESS(subtype)                               \
  template <>                                                                  \
  struct config_value_access<config_value::subtype>                            \
      : default_config_value_access<config_value::subtype> {}

CAF_CONFIG_VALUE_DEFAULT_ACCESS(integer);
CAF_CONFIG_VALUE_DEFAULT_ACCESS(boolean);
CAF_CONFIG_VALUE_DEFAULT_ACCESS(real);
CAF_CONFIG_VALUE_DEFAULT_ACCESS(atom);
CAF_CONFIG_VALUE_DEFAULT_ACCESS(timespan);
CAF_CONFIG_VALUE_DEFAULT_ACCESS(string);
CAF_CONFIG_VALUE_DEFAULT_ACCESS(list);
CAF_CONFIG_VALUE_DEFAULT_ACCESS(dictionary);

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
      return std::all_of(lst->begin(), lst->end(), [](const config_value& x) {
        return caf::holds_alternative<T>(x);
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

/// Implements automagic unboxing of `std::map<std::string, V>` from a
/// homogeneous `config_value::dictionary`.
/// @relates config_value
template <class V>
struct config_value_access<std::map<std::string, V>> {
  using map_type = std::map<std::string, V>;

  using kvp = std::pair<const std::string, config_value>;

  static constexpr bool specialized = true;

  static bool is(const config_value& x) {
    auto lst = caf::get_if<config_value::dictionary>(&x);
    if (lst != nullptr) {
      return std::all_of(lst->begin(), lst->end(), [](const kvp& x) {
        return config_value_access<V>::is(x.second);
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

/// Tries to retrieve the value associated to `name` from `xs`.
/// @relates config_value
template <class T>
optional<T> get_if(const config_value::dictionary* xs,
                   const std::string& name) {
  // Split the name into a path.
  std::vector<std::string> path;
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
T get(const config_value::dictionary& xs, const std::string& name) {
  auto result = get_if<T>(&xs, name);
  if (result)
    return std::move(*result);
  CAF_RAISE_ERROR("invalid type or name found");
}

/// Retrieves the value associated to `name` from `xs` or returns
/// `default_value`.
/// @relates config_value
template <class T, class E = detail::enable_if_t<!std::is_pointer<T>::value>>
T get_or(const config_value::dictionary& xs, const std::string& name,
         const T& default_value) {
  auto result = get_if<T>(&xs, name);
  if (result)
    return std::move(*result);
  return default_value;
}

/// Retrieves the value associated to `name` from `xs` or returns
/// `default_value`.
/// @relates config_value
std::string get_or(const config_value::dictionary& xs, const std::string& name,
                   const char* default_value);

/// Tries to retrieve the value associated to `name` from `xs`.
/// @relates config_value
template <class T>
optional<T> get_if(const config_value_map* xs, const std::string& name) {
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
T get(const config_value_map& xs, const std::string& name) {
  auto result = get_if<T>(&xs, name);
  if (result)
    return std::move(*result);
  CAF_RAISE_ERROR("invalid type or name found");
}

/// Retrieves the value associated to `name` from `xs` or returns
/// `default_value`.
/// @relates config_value
template <class T, class E = detail::enable_if_t<!std::is_pointer<T>::value>>
T get_or(const config_value_map& xs, const std::string& name,
         const T& default_value) {
  auto result = get_if<T>(&xs, name);
  if (result)
    return std::move(*result);
  return default_value;
}

/// Retrieves the value associated to `name` from `xs` or returns
/// `default_value`.
/// @relates config_value
std::string get_or(const config_value_map& xs, const std::string& name,
                   const char* default_value);

/// Tries to retrieve the value associated to `name` from `cfg`.
/// @relates config_value
template <class T>
optional<T> get_if(const actor_system_config* cfg, const std::string& name) {
  return get_if<T>(&content(*cfg), name);
}

/// Retrieves the value associated to `name` from `cfg`.
/// @relates config_value
template <class T>
T get(const actor_system_config& cfg, const std::string& name) {
  return get<T>(content(cfg), name);
}

/// Retrieves the value associated to `name` from `cfg` or returns
/// `default_value`.
/// @relates config_value
template <class T, class E = detail::enable_if_t<!std::is_pointer<T>::value>>
T get_or(const actor_system_config& cfg, const std::string& name,
         const T& default_value) {
  return get_or(content(cfg), name, default_value);
}

std::string get_or(const actor_system_config& cfg, const std::string& name,
                   const char* default_value);

// -- SumType-like access ------------------------------------------------------

/// Delegates to config_value_access for all specialized versions.
template <class T, bool Specialized = config_value_access<T>::specialized>
struct select_config_value_access {
  using type = config_value_access<T>;
};

/// Catches all non-specialized integer types.
template <class T>
struct select_config_value_access<T, false> {
  static_assert(std::is_integral<T>::value,
                "T must be integral or specialize config_value_access");

  struct type {
    static bool is(const config_value& x) {
      using cvi = typename config_value::integer;
      auto ptr = config_value_access<cvi>::get_if(&x);
      return ptr != nullptr && detail::bounds_checker<T>::check(*ptr);
    }

    static optional<T> get_if(const config_value* x) {
      using cvi = typename config_value::integer;
      auto ptr = config_value_access<cvi>::get_if(x);
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

/// @relates config_value
template <class T>
bool holds_alternative(const config_value& x) {
  return select_config_value_access_t<T>::is(x);
}

/// @relates config_value
template <class T>
typename std::conditional<detail::is_primitive_config_value<T>::value, const T*,
                          optional<T>>::type
get_if(const config_value* x) {
  return select_config_value_access_t<T>::get_if(x);
}

/// @relates config_value
template <class T>
typename std::conditional<detail::is_primitive_config_value<T>::value, T*,
                          optional<T>>::type
get_if(config_value* x) {
  return select_config_value_access_t<T>::get_if(x);
}

/// @relates config_value
template <class T>
typename std::conditional<detail::is_primitive_config_value<T>::value, const T&,
                          T>::type
get(const config_value& x) {
  return select_config_value_access_t<T>::get(x);
}

/// @relates config_value
template <class T>
typename std::conditional<detail::is_primitive_config_value<T>::value, T&,
                          T>::type
get(config_value& x) {
  return select_config_value_access_t<T>::get(x);
}

/// @relates config_value
template <class Visitor>
auto visit(Visitor&& f, config_value& x)
-> decltype(visit(std::forward<Visitor>(f), x.get_data())) {
  return visit(std::forward<Visitor>(f), x.get_data());
}

/// @relates config_value
template <class Visitor>
auto visit(Visitor&& f, const config_value& x)
-> decltype(visit(std::forward<Visitor>(f), x.get_data())) {
  return visit(std::forward<Visitor>(f), x.get_data());
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

