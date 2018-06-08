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
#include "caf/deep_to_string.hpp"
#include "caf/default_sum_type_access.hpp"
#include "caf/detail/bounds_checker.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/optional.hpp"
#include "caf/sum_type.hpp"
#include "caf/sum_type_access.hpp"
#include "caf/timestamp.hpp"
#include "caf/variant.hpp"

namespace caf {

/// A type for config parameters with similar interface to a `variant`. This
/// type is not implemented as a simple variant alias because variants cannot
/// contain lists of themselves.
class config_value {
public:
  // -- friends ----------------------------------------------------------------

  friend struct default_sum_type_access<config_value>;

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

  // -- properties -------------------------------------------------------------

  /// Converts the value to a list with one element. Does nothing if the value
  /// already is a list.
  void convert_to_list();

  /// Appends `x` to a list. Converts this config value to a list first by
  /// calling `convert_to_list` if needed.
  void append(config_value x);

  /// Returns a human-readable type name of the current value.
  const char* type_name() const noexcept;

  /// Returns a human-readable type name for `T`.
  template <class T>
  static const char* type_name_of() noexcept {
    using namespace detail;
    static constexpr auto index = tl_index_of<types, T>::value;
    static_assert(index != -1, "T is not a valid config value type");
    return type_name_at_index(static_cast<size_t>(index));
  }

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

/// @relates config_value
template <class T>
struct config_value_access;

/// @relates config_value
template <class T>
auto holds_alternative(const config_value& x)
-> decltype(config_value_access<T>::is(x)) {
  return config_value_access<T>::is(x);
}

/// @relates config_value
template <class T>
auto get_if(const config_value* x)
-> decltype(config_value_access<T>::get_if(x)) {
  return config_value_access<T>::get_if(x);
}

/// @relates config_value
template <class T>
auto get(const config_value& x) -> decltype(config_value_access<T>::get(x)) {
  return config_value_access<T>::get(x);
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

/// @relates config_value_access
template <class T>
struct default_config_value_access {
  static bool is(const config_value& x) {
    return holds_alternative<T>(x.get_data());
  }

  static const T* get_if(const config_value* x) {
    return caf::get_if<T>(&(x->get_data()));
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

/// Checks whether `x` is a `config_value::integer` that can convert to `T`.
/// @relates config_value
template <class T>
detail::enable_if_t<std::is_integral<T>::value
                    && !std::is_same<T, typename config_value::integer>::value
                    && !std::is_same<T, bool>::value,
                    bool>
holds_alternative(const config_value& x) {
  using cvi = typename config_value::integer;
  auto ptr = config_value_access<cvi>::get_if(&x);
  return ptr != nullptr && detail::bounds_checker<T>::check(*ptr);
}

/// Tries to convert the value of `x` to `T`.
/// @relates config_value
template <class T>
detail::enable_if_t<std::is_integral<T>::value
                    && !std::is_same<T, typename config_value::integer>::value
                    && !std::is_same<T, bool>::value,
                    optional<T>>
get_if(const config_value* x) {
  using cvi = typename config_value::integer;
  auto ptr = config_value_access<cvi>::get_if(x);
  if (ptr != nullptr && detail::bounds_checker<T>::check(*ptr))
    return static_cast<T>(*ptr);
  return none;
}

/// Converts the value of `x` to `T`.
/// @relates config_value
template <class T>
detail::enable_if_t<std::is_integral<T>::value
                    && !std::is_same<T, typename config_value::integer>::value
                    && !std::is_same<T, bool>::value,
                    T>
get(const config_value& x) {
  auto res = get_if<T>(&x);
  if (res)
    return *res;
  CAF_RAISE_ERROR("invalid type found");
}

/// Implements automagic unboxing of `std::vector<T>` from a homogeneous
/// `config_value::list`.
/// @relates config_value
template <class T>
struct config_value_access<std::vector<T>> {
  using vector_type = std::vector<T>;

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

/// Retrieves.
/// @relates config_value
template <class T>
optional<T> get_if(const config_value::dictionary* xs,
                   std::initializer_list<std::string> path) {
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

/// @relates config_value
template <class T>
optional<T> get_if(const config_value::dictionary* xs, std::string path) {
  return get_if<T>(xs, {path});
}

/// @relates config_value
template <class T>
T get(const config_value::dictionary& xs,
      std::initializer_list<std::string> path) {
  auto result = get_if<T>(&xs, std::move(path));
  if (!result)
    CAF_RAISE_ERROR("invalid type found");
  return std::move(*result);
}

/// @relates config_value
template <class T>
T get(const config_value::dictionary& xs, std::string path) {
  return get<T>(xs, {path});
}

/// @relates config_value
template <class T>
T get_or(const config_value::dictionary& xs,
         std::initializer_list<std::string> path, const T& default_value) {
  auto result = get<T>(xs, std::move(path));
  if (result)
    return *result;
  return default_value;
}

/// @relates config_value
template <class T>
T get_or(const config_value::dictionary& xs, std::string path,
         const T& default_value) {
  return get_or(xs, {path}, default_value);
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

