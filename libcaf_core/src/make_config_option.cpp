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

#include "caf/make_config_option.hpp"

namespace caf {

namespace {

using meta_state = config_option::meta_state;

error bool_check(const config_value& x) {
  if (holds_alternative<bool>(x))
    return none;
  return make_error(pec::type_mismatch);
}

void bool_store(void* ptr, const config_value& x) {
  *static_cast<bool*>(ptr) = get<bool>(x);
}

void bool_store_neg(void* ptr, const config_value& x) {
  *static_cast<bool*>(ptr) = !get<bool>(x);
}

meta_state bool_meta{bool_check, bool_store, detail::type_name<bool>(), true};

meta_state bool_neg_meta{bool_check, bool_store_neg, detail::type_name<bool>(),
                        true};

} // namespace anonymous

config_option make_negated_config_option(bool& storage, const char* category,
                                         const char* name,
                                         const char* description) {
  return {category, name, description, &bool_neg_meta, &storage};
}

template <>
config_option make_config_option<bool>(const char* category, const char* name,
                                       const char* description) {
  return {category, name, description, &bool_meta};
}

template <>
config_option make_config_option<bool>(bool& storage, const char* category,
                                       const char* name,
                                       const char* description) {
  return {category, name, description, &bool_meta, &storage};
}

} // namespace caf
