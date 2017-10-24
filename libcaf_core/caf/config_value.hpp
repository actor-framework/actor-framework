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
#include "caf/timestamp.hpp"
#include "caf/variant.hpp"

#include "caf/detail/comparable.hpp"

namespace caf {

/// A type for config parameters with similar interface to a `variant`. This
/// type is not implemented as a simple variant alias because variants cannot
/// contain lists of themselves.
class config_value {
public:
  using T0 = int64_t;
  using T1 = bool;
  using T2 = double;
  using T3 = atom_value;
  using T4 = timespan;
  using T5 = std::string;
  using T6 = std::vector<config_value>;
  using T7 = std::map<std::string, config_value>;

  using type0 = T0;

  using types = detail::type_list<T0, T1, T2, T3, T4, T5, T6, T7>;

  using variant_type = variant<T0, T1, T2, T3, T4, T5, T6, T7>;

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

  /// Converts the value to a list with one element. Does nothing if the value
  /// already is a list.
  void convert_to_list();

  /// Appends `x` to a list. Converts this config value to a list first by
  /// calling `convert_to_list` if needed.
  void append(config_value x);

  inline size_t index() const {
    return data_.index();
  }

  inline bool valueless_by_exception() const {
    return data_.valueless_by_exception();
  }

  template <class Visitor>
  auto apply(Visitor&& visitor) -> decltype(visitor(std::declval<T0&>())) {
    return data_.apply(visitor);
  }

  template <class Visitor>
  auto apply(Visitor&& visitor) const
  -> decltype(visitor(std::declval<const T0&>())) {
    return data_.apply(visitor);
  }

  template <int Pos>
  const typename detail::tl_at<types, Pos>::type&
  get(std::integral_constant<int, Pos> token) const {
    return data_.get(token);
  }

  template <int Pos>
  typename detail::tl_at<types, Pos>::type&
  get(std::integral_constant<int, Pos> token) {
    return data_.get(token);
  }

  inline variant_type& data() {
    return data_;
  }

  inline const variant_type& data() const {
    return data_;
  }

  /// @endcond

private:
  // -- auto conversion of related types ---------------------------------------

  inline void set(bool x) {
    data_ = x;
  }

  inline void set(const char* x) {
    data_ = std::string{x};
  }

  template <class T>
  detail::enable_if_t<
    detail::is_one_of<detail::decay_t<T>, T2, T3, T4, T5, T6, T7>::value>
  set(T&& x) {
    data_ = std::forward<T>(x);
  }

  template <class T>
  detail::enable_if_t<std::is_integral<T>::value> set(T x) {
    data_ = static_cast<int64_t>(x);
  }

  inline void convert(timespan x) {
    data_ = x;
  }

  template <class Rep, class Ratio>
  void convert(std::chrono::duration<Rep, Ratio> x) {
    data_ = std::chrono::duration_cast<timespan>(x);
  }

  // -- member variables -------------------------------------------------------

  variant_type data_;
};

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
template <class Visitor>
auto visit(Visitor&& f, config_value& x)
  -> decltype(f(std::declval<int64_t&>())) {
  return visit(std::forward<Visitor>(f), x.data());
}

/// @relates config_value
template <class Visitor>
auto visit(Visitor&& f, const config_value& x)
  -> decltype(f(std::declval<const int64_t&>())) {
  return visit(std::forward<Visitor>(f), x.data());
}

/// @relates config_value
template <class T>
bool holds_alternative(const config_value& x) {
  return holds_alternative<T>(x.data());
}

/// @relates config_value
template <class T>
T& get(config_value& x) {
  return get<T>(x.data());
}

/// @relates config_value
template <class T>
const T& get(const config_value& x) {
  return get<T>(x.data());
}

/// @relates config_value
template <class T>
T* get_if(config_value* x) {
  return x != nullptr ? get_if<T>(&(x->data())) : nullptr;
}

/// @relates config_value
template <class T>
const T* get_if(const config_value* x) {
  return x != nullptr ? get_if<T>(&(x->data())) : nullptr;
}

/// @relates config_value
inline std::string to_string(const config_value& x) {
  return deep_to_string(x.data());
}

/// @relates config_value
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, config_value& x) {
  return f(meta::type_name("config_value"), x.data());
}

} // namespace caf

