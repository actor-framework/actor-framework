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

#include <memory>

#include "caf/config_option.hpp"
#include "caf/config_value.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/pec.hpp"
#include "caf/string_view.hpp"

namespace caf {

namespace detail {

template <class T>
error check_impl(const config_value& x) {
  if (holds_alternative<T>(x))
    return none;
  return make_error(pec::type_mismatch);
}

template <class T>
void store_impl(void* ptr, const config_value& x) {
  *static_cast<T*>(ptr) = get<T>(x);
}

template <class T>
config_value get_impl(const void* ptr) {
  return config_value{*reinterpret_cast<const T*>(ptr)};
}

template <class T>
expected<config_value> parse_impl(T* ptr, string_view str) {
  if (!ptr) {
    T tmp;
    return parse_impl(&tmp, str);
  }
  using trait = select_config_value_access_t<T>;
  config_value::parse_state ps{str.begin(), str.end()};
  trait::parse_cli(ps, *ptr);
  if (ps.code != pec::success)
    return ps.make_error(ps.code);
  return config_value{trait::convert(*ptr)};
}

expected<config_value> parse_impl(std::string* ptr, string_view str);

template <class T>
expected<config_value> parse_impl_delegate(void* ptr, string_view str) {
  return parse_impl(reinterpret_cast<T*>(ptr), str);
}

template <class T>
config_option::meta_state* option_meta_state_instance() {
  using trait = select_config_value_access_t<T>;
  static config_option::meta_state obj{check_impl<T>, store_impl<T>,
                                       get_impl<T>, parse_impl_delegate<T>,
                                       trait::type_name()};
  return &obj;
}

} // namespace detail

/// Creates a config option that synchronizes with `storage`.
template <class T>
config_option make_config_option(string_view category, string_view name,
                                 string_view description) {
  return {category, name, description, detail::option_meta_state_instance<T>()};
}

/// Creates a config option that synchronizes with `storage`.
template <class T>
config_option make_config_option(T& storage, string_view category,
                                 string_view name, string_view description) {
  return {category, name, description, detail::option_meta_state_instance<T>(),
          std::addressof(storage)};
}

// -- backward compatbility, do not use for new code ! -------------------------

// Inverts the value when writing to `storage`.
config_option make_negated_config_option(bool& storage, string_view category,
                                         string_view name,
                                         string_view description);

// Reads timespans, but stores an integer representing microsecond resolution.
config_option make_us_resolution_config_option(size_t& storage,
                                               string_view category,
                                               string_view name,
                                               string_view description);

// Reads timespans, but stores an integer representing millisecond resolution.
config_option make_ms_resolution_config_option(size_t& storage,
                                               string_view category,
                                               string_view name,
                                               string_view description);

} // namespace caf
