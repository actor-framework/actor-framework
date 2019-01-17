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

#include "caf/config_value.hpp"
#include "caf/dictionary.hpp"
#include "caf/optional.hpp"
#include "caf/raise_error.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// Software options stored as key-value pairs.
/// @relates config_value
using settings = dictionary<config_value>;

/// Tries to retrieve the value associated to `name` from `xs`.
/// @relates config_value
template <class T>
optional<T> get_if(const settings* xs, string_view name) {
  // Access the key directly unless the user specified a dot-separated path.
  auto pos = name.find('.');
  if (pos == std::string::npos) {
    auto i = xs->find(name);
    if (i == xs->end())
      return none;
    // We can't simply return the result here, because it might be a pointer.
    auto result = get_if<T>(&i->second);
    if (result)
      return *result;
    return none;
  }
  // We're dealing with a `<category>.<key>`-formatted string, extract the
  // sub-settings by category and recurse.
  auto i = xs->find(name.substr(0, pos));
  if (i == xs->end() || !holds_alternative<config_value::dictionary>(i->second))
    return none;
  return get_if<T>(&get<config_value::dictionary>(i->second),
                   name.substr(pos + 1));
}

template <class T>
T get(const settings& xs, string_view name) {
  auto result = get_if<T>(&xs, name);
  CAF_ASSERT(result != none);
  return std::move(*result);
}

template <class T, class = typename std::enable_if<
                     !std::is_pointer<T>::value
                     && !std::is_convertible<T, string_view>::value>::type>
T get_or(const settings& xs, string_view name, T default_value) {
  auto result = get_if<T>(&xs, name);
  if (result)
    return std::move(*result);
  return std::move(default_value);
}

std::string get_or(const settings& xs, string_view name,
                   string_view default_value);

/// @private
config_value& put_impl(settings& dict, const std::vector<string_view>& path,
                       config_value& value);

/// @private
config_value& put_impl(settings& dict, string_view key, config_value& value);

/// Converts `value` to a `config_value` and assigns it to `key`.
/// @param dict Dictionary of key-value pairs.
/// @param key Human-readable nested keys in the form `category.key`.
/// @param value New value for given `key`.
template <class T>
config_value& put(settings& dict, string_view key, T&& value) {
  config_value tmp{std::forward<T>(value)};
  return put_impl(dict, key, tmp);
}

/// Inserts a new list named `name` into the dictionary `xs` and returns
/// a reference to it. Overrides existing entries with the same name.
config_value::list& put_list(settings& xs, std::string name);

/// Inserts a new list named `name` into the dictionary `xs` and returns
/// a reference to it. Overrides existing entries with the same name.
config_value::dictionary& put_dictionary(settings& xs, std::string name);

} // namespace caf
