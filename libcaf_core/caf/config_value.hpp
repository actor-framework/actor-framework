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
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
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

/// Enable `holds_alternative`, `get`, `get_if`, and `visit` for `config_value`.
/// @relates config_value
/// @relates SumType
template <>
struct sum_type_access<config_value> : default_sum_type_access<config_value> {};

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

