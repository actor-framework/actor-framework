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

#include "caf/settings.hpp"

namespace caf {

std::string get_or(const settings& xs, string_view name,
                   string_view default_value) {
  auto result = get_if<std::string>(&xs, name);
  if (result)
    return std::move(*result);
  return std::string{default_value.begin(), default_value.end()};
}

void put_impl(settings& dict, const std::vector<string_view>& path,
              config_value& value) {
  // Sanity check.
  if (path.empty())
    return;
  // Navigate path.
  auto last = path.end();
  auto back = last - 1;
  auto current = &dict;
  // Resolve path by navigating the map-of-maps of create the necessary layout
  // when needed.
  for (auto i = path.begin(); i != back; ++i) {
    auto iter = current->emplace(*i, settings{}).first;
    if (auto val = get_if<settings>(&iter->second)) {
      current = val;
    } else {
      iter->second = settings{};
      current = &get<settings>(iter->second);
    }
  }
  // Set key-value pair on the leaf.
  current->insert_or_assign(*back, std::move(value));
}

void put_impl(settings& dict, string_view key, config_value& value) {
  std::vector<string_view> path;
  split(path, key, ".");
  put_impl(dict, path, value);
}

config_value::list& put_list(settings& xs, std::string name) {
  auto i = xs.insert_or_assign(std::move(name), config_value::list{});
  return get<config_value::list>(i.first->second);
}

config_value::dictionary& put_dictionary(settings& xs, std::string name) {
  auto i = xs.insert_or_assign(std::move(name), settings{});
  return get<config_value::dictionary>(i.first->second);
}

} // namespace caf
