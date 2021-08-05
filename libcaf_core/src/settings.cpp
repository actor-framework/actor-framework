// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/settings.hpp"

#include "caf/string_algorithms.hpp"

namespace caf {

// note: to_string is implemented in config_value.cpp

const config_value* get_if(const settings* xs, std::string_view name) {
  // Access the key directly unless the user specified a dot-separated path.
  auto pos = name.find('.');
  if (pos == std::string::npos) {
    auto i = xs->find(name);
    if (i == xs->end())
      return nullptr;
    // We can't simply return the result here, because it might be a pointer.
    return &i->second;
  }
  // We're dealing with a `<category>.<key>`-formatted string, extract the
  // sub-settings by category and recurse.
  auto i = xs->find(name.substr(0, pos));
  if (i == xs->end() || !holds_alternative<config_value::dictionary>(i->second))
    return nullptr;
  return get_if(&get<config_value::dictionary>(i->second),
                name.substr(pos + 1));
}

expected<std::string> get_or(const settings& xs, std::string_view name,
                             const char* fallback) {
  if (auto ptr = get_if(&xs, name))
    return get_as<std::string>(*ptr);
  else
    return {std::string{fallback}};
}

config_value& put_impl(settings& dict,
                       const std::vector<std::string_view>& path,
                       config_value& value) {
  // Sanity check.
  CAF_ASSERT(!path.empty());
  // TODO: We implicitly swallow the `global.` suffix as a hotfix, but we
  // actually should drop `global.` on the upper layers.
  if (path.front() == "global") {
    std::vector<std::string_view> new_path{path.begin() + 1, path.end()};
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

config_value& put_impl(settings& dict, std::string_view key,
                       config_value& value) {
  std::vector<std::string_view> path;
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
