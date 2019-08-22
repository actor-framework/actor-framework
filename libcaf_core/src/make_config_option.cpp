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

namespace detail {

expected<config_value> parse_impl(std::string* ptr, string_view str) {
  // Parse quoted strings, otherwise consume the entire string.
  auto e = str.end();
  auto i = std::find_if(str.begin(), e, [](char c) { return !isspace(c); });
  if (i == e) {
    if (ptr != nullptr)
      ptr->assign(i, e);
    return config_value{std::string{i, e}};
  }
  if (*i == '"') {
    if (ptr == nullptr) {
      std::string tmp;
      if (auto err = parse(str, tmp))
        return err;
      return config_value{std::move(tmp)};
    } else {
      if (auto err = parse(str, *ptr))
        return err;
      return config_value{*ptr};
    }
  }
  if (ptr != nullptr)
    ptr->assign(str.begin(), str.end());
  return config_value{std::string{str.begin(), str.end()}};
}

} // namespace detail

namespace {

using meta_state = config_option::meta_state;

void bool_store_neg(void* ptr, const config_value& x) {
  *static_cast<bool*>(ptr) = !get<bool>(x);
}

config_value bool_get_neg(const void* ptr) {
  return config_value{!*static_cast<const bool*>(ptr)};
}

meta_state bool_neg_meta{detail::check_impl<bool>, bool_store_neg, bool_get_neg,
                         nullptr, detail::type_name<bool>()};

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

meta_state ms_res_meta{[](const config_value& x) -> error {
                         if (holds_alternative<timespan>(x))
                           return none;
                         return make_error(pec::type_mismatch);
                       },
                       [](void* ptr, const config_value& x) {
                         *static_cast<size_t*>(ptr) = static_cast<size_t>(
                           get<timespan>(x).count() / 1000000);
                       },
                       [](const void* ptr) -> config_value {
                         auto ival = static_cast<int64_t>(
                           *static_cast<const size_t*>(ptr));
                         timespan val{ival * 1000000};
                         return config_value{val};
                       },
                       nullptr, detail::type_name<timespan>()};

} // namespace

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
