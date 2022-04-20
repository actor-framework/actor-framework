// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/settings.hpp"

#include "caf/string_algorithms.hpp"

namespace caf {

// note: to_string is implemented in config_value.cpp

const config_value* get_if(const settings* xs, string_view name) {
  // The 'global' category is special in the sense that it refers back to the
  // root. Somewhat like '::foo' in C++. This means we can just drop it here.
  using namespace caf::literals;
  auto gl = "global."_sv;
  if (starts_with(name, gl))
    name.remove_prefix(gl.size());
  // Climb down the tree. In each step, we resolve `xs` and `name` to the next
  // level until there is no category left to resolve. At that point it's a
  // trivial name lookup.
  for (;;) {
    if (auto pos = name.find('.'); pos == std::string::npos) {
      if (auto i = xs->find(name); i == xs->end())
        return nullptr;
      else
        return &i->second;
    } else {
      auto category = name.substr(0, pos);
      name.remove_prefix(pos + 1);
      if (auto i = xs->find(category);
          i == xs->end() || !holds_alternative<settings>(i->second)) {
        return nullptr;
      } else {
        xs = std::addressof(get<settings>(i->second));
      }
    }
  }
}

expected<std::string> get_or(const settings& xs, string_view name,
                             const char* fallback) {
  if (auto ptr = get_if(&xs, name))
    return get_as<std::string>(*ptr);
  else
    return {std::string{fallback}};
}

config_value& put_impl(settings& dict, const std::vector<string_view>& path,
                       config_value& value) {
  // Sanity check.
  CAF_ASSERT(!path.empty());
  // Like in get_if: we always drop a 'global.' suffix.
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

config_value& put_impl(settings& dict, string_view name, config_value& value) {
  // Like in get_if: we always drop a 'global.' suffix.
  using namespace caf::literals;
  auto gl = "global."_sv;
  if (starts_with(name, gl))
    name.remove_prefix(gl.size());
  // Climb down the tree, similar to get_if. Only this time, we create the
  // necessary structure as we go until there is no category left to resolve. At
  // that point it's a trivial insertion (override).
  auto xs = &dict;
  for (;;) {
    if (auto pos = name.find('.'); pos == string_view::npos) {
      return xs->insert_or_assign(name, std::move(value)).first->second;
    } else {
      auto category = name.substr(0, pos);
      name.remove_prefix(pos + 1);
      if (auto i = xs->find(category); i == xs->end()) {
        auto j = xs->emplace(category, settings{}).first;
        xs = std::addressof(get<settings>(j->second));
      } else if (!holds_alternative<settings>(i->second)) {
        i->second = settings{};
        xs = std::addressof(get<settings>(i->second));
      } else {
        xs = std::addressof(get<settings>(i->second));
      }
    }
  }
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
