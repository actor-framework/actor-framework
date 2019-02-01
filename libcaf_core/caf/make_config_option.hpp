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
#include "caf/detail/type_name.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/pec.hpp"
#include "caf/string_view.hpp"

namespace caf {

namespace detail {

template <class T>
error default_config_option_check(const config_value& x) {
  if (holds_alternative<T>(x))
    return none;
  return make_error(pec::type_mismatch);
}

template <class T>
void default_config_option_store(void* ptr, const config_value& x) {
  *static_cast<T*>(ptr) = get<T>(x);
}

template <class T>
expected<config_value> default_config_option_parse(void* ptr, string_view str) {
  auto result = config_value::parse(str.begin(), str.end());
  if (result) {
    if (!holds_alternative<T>(*result))
      return make_error(pec::type_mismatch);
    if (ptr != nullptr)
      *static_cast<T*>(ptr) = get<T>(*result);
  }
  return result;
}

template <class T>
config_option::meta_state* option_meta_state_instance() {
  static config_option::meta_state obj{
    default_config_option_check<T>, default_config_option_store<T>, nullptr,
    default_config_option_parse<T>, detail::type_name<T>()};
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

// -- specializations for common types -----------------------------------------

#define CAF_SPECIALIZE_META_STATE(type)                                        \
  extern config_option::meta_state type##_meta_state;                          \
  template <>                                                                  \
  inline config_option::meta_state* option_meta_state_instance<type>() {       \
    return &type##_meta_state;                                                 \
  }

namespace detail {

CAF_SPECIALIZE_META_STATE(atom_value)

CAF_SPECIALIZE_META_STATE(bool)

CAF_SPECIALIZE_META_STATE(size_t)

extern config_option::meta_state string_meta_state;

template <>
inline config_option::meta_state* option_meta_state_instance<std::string>() {
  return &string_meta_state;
}

} // namespace detail

#undef CAF_SPECIALIZE_META_STATE

} // namespace caf
