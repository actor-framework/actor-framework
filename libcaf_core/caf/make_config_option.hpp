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
#include "caf/fwd.hpp"
#include "caf/pec.hpp"

namespace caf {

namespace detail {

template <class T>
struct option_meta_state {
  static config_option::meta_state instance;
};

template <class T>
config_option::meta_state option_meta_state<T>::instance{
  [](const config_value& x) -> error {
    if (holds_alternative<T>(x))
      return none;
    return make_error(pec::type_mismatch);
  },
  [](void* ptr, const config_value& x) {
    *static_cast<T*>(ptr) = get<T>(x);
  },
  nullptr,
  detail::type_name<T>()
};

} // namespace detail

/// Creates a config option that synchronizes with `storage`.
template <class T>
config_option make_config_option(string_view category, string_view name,
                                 string_view description) {
  return {category, name, description, &detail::option_meta_state<T>::instance};
}

/// Creates a config option that synchronizes with `storage`.
template <class T>
config_option make_config_option(T& storage, string_view category,
                                 string_view name, string_view description) {
  return {category, name, description, &detail::option_meta_state<T>::instance,
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

#define CAF_SPECIALIZE_MAKE_CONFIG_OPTION(type)                                \
  template <>                                                                  \
  config_option make_config_option<std::string>(                               \
    std::string & storage, string_view category, string_view name,             \
    string_view description);                                                  \
  template <>                                                                  \
  config_option make_config_option<std::string>(                               \
    string_view category, string_view name, string_view description)

CAF_SPECIALIZE_MAKE_CONFIG_OPTION(atom_value);

CAF_SPECIALIZE_MAKE_CONFIG_OPTION(bool);

CAF_SPECIALIZE_MAKE_CONFIG_OPTION(size_t);

CAF_SPECIALIZE_MAKE_CONFIG_OPTION(std::string);

} // namespace caf
