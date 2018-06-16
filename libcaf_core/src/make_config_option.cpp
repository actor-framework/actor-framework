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

using vtbl_type = config_option::vtbl_type;

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

std::string bool_name() {
  return detail::type_name<bool>();
}

constexpr vtbl_type bool_vtbl{bool_check, bool_store, bool_name};

constexpr vtbl_type bool_neg_vtbl{bool_check, bool_store_neg, bool_name};

} // namespace anonymous

config_option make_negated_config_option(bool& storage, const char* category,
                                         const char* name,
                                         const char* description) {
  return {category, name, description, true, bool_neg_vtbl, &storage};
}

template <>
config_option make_config_option<bool>(const char* category, const char* name,
                                       const char* description) {
  return {category, name, description, true, bool_vtbl};
}

template <>
config_option make_config_option<bool>(bool& storage, const char* category,
                                       const char* name,
                                       const char* description) {
  return {category, name, description, true, bool_vtbl, &storage};
}

} // namespace caf
