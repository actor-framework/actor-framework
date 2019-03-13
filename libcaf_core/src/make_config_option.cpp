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

#include <ctype.h>

#include "caf/config_value.hpp"
#include "caf/optional.hpp"

#define DEFAULT_META(type, parse_fun)                                          \
  config_option::meta_state type##_meta_state{                                 \
    default_config_option_check<type>, default_config_option_store<type>,      \
    get_impl<type>, parse_fun, detail::type_name<type>()};

using std::string;

namespace caf {

namespace {

using meta_state = config_option::meta_state;

template <class T>
config_value get_impl(const void* ptr) {
  CAF_ASSERT(ptr != nullptr);
  return config_value{*static_cast<const T*>(ptr)};
}

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

config_value bool_get(const void* ptr) {
  return config_value{*static_cast<const bool*>(ptr)};
}

config_value bool_get_neg(const void* ptr) {
  return config_value{!*static_cast<const bool*>(ptr)};
}

meta_state bool_neg_meta{bool_check, bool_store_neg, bool_get_neg, nullptr,
                         detail::type_name<bool>()};

meta_state us_res_meta{
  [](const config_value& x) -> error {
    if (holds_alternative<timespan>(x))
      return none;
    return make_error(pec::type_mismatch);
  },
  [](void* ptr, const config_value& x) {
    *static_cast<size_t*>(ptr) = static_cast<size_t>(get<timespan>(x).count())
                                 / 1000;
  },
  [](const void* ptr) -> config_value {
    auto ival = static_cast<int64_t>(*static_cast<const size_t*>(ptr));
    timespan val{ival * 1000};
    return config_value{val};
  },
  nullptr,
  detail::type_name<timespan>()
};

meta_state ms_res_meta{
  [](const config_value& x) -> error {
    if (holds_alternative<timespan>(x))
      return none;
    return make_error(pec::type_mismatch);
  },
  [](void* ptr, const config_value& x) {
    *static_cast<size_t*>(ptr) = static_cast<size_t>(get<timespan>(x).count()
                                                     / 1000000);
  },
  [](const void* ptr) -> config_value {
    auto ival = static_cast<int64_t>(*static_cast<const size_t*>(ptr));
    timespan val{ival * 1000000};
    return config_value{val};
  },
  nullptr,
  detail::type_name<timespan>()};

expected<config_value> parse_atom(void* ptr, string_view str) {
  if (str.size() > 10)
    return make_error(pec::too_many_characters);
  auto is_illegal = [](char c) {
    return !(isalnum(c) || c == '_' || c == ' ');
  };
  auto i = std::find_if(str.begin(), str.end(), is_illegal);
  if (i != str.end())
    return make_error(pec::unexpected_character,
                      std::string{"invalid character: "} + *i);
  auto result = atom_from_string(str);
  if (ptr != nullptr)
    *reinterpret_cast<atom_value*>(ptr) = result;
  return config_value{result};
}

expected<config_value> parse_string(void* ptr, string_view str) {
  std::string result{str.begin(), str.end()};
  if (ptr != nullptr)
    *reinterpret_cast<std::string*>(ptr) = result;
  return config_value{std::move(result)};
}

} // namespace

namespace detail {

DEFAULT_META(atom_value, parse_atom)

DEFAULT_META(size_t, default_config_option_parse<size_t>)

DEFAULT_META(string, parse_string)

config_option::meta_state bool_meta_state{bool_check, bool_store, bool_get,
                                          default_config_option_parse<bool>,
                                          detail::type_name<bool>()};

} // namespace detail

config_option make_negated_config_option(bool& storage, string_view category,
                                         string_view name,
                                         string_view description) {
  return {category, name, description, &bool_neg_meta, &storage};
}

config_option make_us_resolution_config_option(size_t& storage,
                                               string_view category,
                                               string_view name,
                                               string_view description) {
  return {category, name, description, &us_res_meta, &storage};
}

config_option make_ms_resolution_config_option(size_t& storage,
                                               string_view category,
                                               string_view name,
                                               string_view description) {
  return {category, name, description, &ms_res_meta, &storage};
}

} // namespace caf
