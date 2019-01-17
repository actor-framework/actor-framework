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

#include "caf/string_algorithms.hpp"

namespace caf {

std::string get_or(const settings& xs, string_view name,
                   string_view default_value) {
  auto result = get_if<std::string>(&xs, name);
  if (result)
    return std::move(*result);
  return std::string{default_value.begin(), default_value.end()};
}

config_value& put_impl(settings& dict, const std::vector<string_view>& path,
                       config_value& value) {
  // Sanity check.
  CAF_ASSERT(!path.empty());
  // TODO: We implicitly swallow the `global.` suffix as a hotfix, but we
  // actually should drop `global.` on the upper layers.
  if (path.front() == "global") {
    std::vector<string_view> new_path{path.begin() + 1, path.end()};
    return put_impl(dict, new_path, value);
  }
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
  auto iter = current->insert_or_assign(*back, std::move(value)).first;
  return iter->second;
}

config_value& put_impl(settings& dict, string_view key, config_value& value) {
  std::vector<string_view> path;
  split(path, key, ".");
  return put_impl(dict, path, value);
}

config_value::list& put_list(settings& xs, std::string name) {
  config_value tmp{config_value::list{}};
  auto& result = put_impl(xs, name, tmp);
  return get<config_value::list>(result);
}

config_value::dictionary& put_dictionary(settings& xs, std::string name) {
  config_value tmp{settings{}};
  auto& result = put_impl(xs, name, tmp);
  return get<config_value::dictionary>(result);
}

} // namespace caf
