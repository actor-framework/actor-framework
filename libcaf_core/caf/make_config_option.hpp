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

#include "caf/config_option.hpp"
#include "caf/config_value.hpp"
#include "caf/detail/type_name.hpp"
#include "caf/pec.hpp"

namespace caf {

/// Creates a config option that synchronizes with `storage`.
template <class T>
config_option make_config_option(const char* category, const char* name,
                                 const char* description) {
  static config_option::meta_state meta{
    [](const config_value& x) -> error {
      if (holds_alternative<T>(x))
        return none;
      return make_error(pec::type_mismatch);
    },
    nullptr,
    detail::type_name<T>(),
    false
  };
  return {category, name, description, &meta};
}

/// Creates a config option that synchronizes with `storage`.
template <class T>
config_option make_config_option(T& storage, const char* category,
                                 const char* name, const char* description) {
  static config_option::meta_state meta{
    [](const config_value& x) -> error {
      if (holds_alternative<T>(x))
        return none;
      return make_error(pec::type_mismatch);
    },
    [](void* ptr, const config_value& x) {
      *static_cast<T*>(ptr) = get<T>(x);
    },
    detail::type_name<T>(),
    false
  };
  return {category, name, description, &meta, &storage};
}

// -- backward compatbility, do not use for new code ! -------------------------

// Inverts the value when writing to `storage`.
config_option make_negated_config_option(bool& storage, const char* category,
                                         const char* name,
                                         const char* description);

// -- specializations for common types.

template <>
config_option make_config_option<bool>(const char* category, const char* name,
                                       const char* description);

template <>
config_option make_config_option<bool>(bool& storage, const char* category,
                                       const char* name,
                                       const char* description);

} // namespace caf
