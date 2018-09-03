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

#include "caf/config_value.hpp"
#include "caf/optional.hpp"

#define DEFAULT_META(type)                                                     \
  config_option::meta_state type##_meta_state {                                \
    check_impl<type>, store_impl<type>, get_impl<type>,                        \
    detail::type_name<type>()                                                  \
  };                                                                           \

using std::string;

namespace caf {

namespace {

using meta_state = config_option::meta_state;

template <class T>
error check_impl(const config_value& x) {
  if (holds_alternative<T>(x))
    return none;
  return make_error(pec::type_mismatch);
}

template <class T>
void store_impl(void* ptr, const config_value& x) {
  CAF_ASSERT(ptr != nullptr);
  *static_cast<T*>(ptr) = get<T>(x);
}

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

meta_state bool_neg_meta{bool_check, bool_store_neg, bool_get_neg,
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
  detail::type_name<timespan>()
};

meta_state ms_res_meta{
  [](const config_value& x) -> error {
    if (holds_alternative<timespan>(x))
      return none;
    return make_error(pec::type_mismatch);
  },
  [](void* ptr, const config_value& x) {
    *static_cast<size_t*>(ptr) = get<timespan>(x).count() / 1000000;
  },
  [](const void* ptr) -> config_value {
    auto ival = static_cast<int64_t>(*static_cast<const size_t*>(ptr));
    timespan val{ival * 1000000};
    return config_value{val};
  },
  detail::type_name<timespan>()
};

} // namespace

namespace detail {

DEFAULT_META(atom_value)

DEFAULT_META(size_t)

DEFAULT_META(string)

config_option::meta_state bool_meta_state{
  bool_check, bool_store, bool_get, detail::type_name<bool>()
};

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
