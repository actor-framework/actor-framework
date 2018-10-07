/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config_value.hpp"

#include "caf/detail/ini_consumer.hpp"
#include "caf/detail/parser/read_ini.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/expected.hpp"
#include "caf/pec.hpp"

namespace caf {

namespace {

const char* type_names[] {
  "integer",
  "boolean",
  "real",
  "atom",
  "timespan",
  "uri",
  "string",
  "list",
  "dictionary",
};

} // namespace <anonymous>

// -- constructors, destructors, and assignment operators ----------------------

config_value::~config_value() {
  // nop
}

// -- parsing ------------------------------------------------------------------

expected<config_value> config_value::parse(string_view::iterator first,
                                           string_view::iterator last) {
  using namespace detail;
  auto i = first;
  // Sanity check.
  if (i == last)
    return make_error(pec::unexpected_eof);
  // Skip to beginning of the argument.
  while (isspace(*i))
    if (++i == last)
      return make_error(pec::unexpected_eof);
  // Dispatch to parser.
  parser::state<string_view::iterator> res;
  detail::ini_value_consumer f;
  res.i = i;
  res.e = last;
  parser::read_ini_value(res, f);
  if (res.code == pec::success)
    return std::move(f.result);
  // Assume an unescaped string unless the first character clearly indicates
  // otherwise.
  switch (*i) {
    case '[':
    case '{':
    case '"':
    case '\'':
      return make_error(res.code);
    default:
      if (isdigit(*i))
        return make_error(res.code);
      return config_value{std::string{first, last}};
  }
}

expected<config_value> config_value::parse(string_view str) {
  return parse(str.begin(), str.end());
}

// -- properties ---------------------------------------------------------------

void config_value::convert_to_list() {
  if (holds_alternative<list>(data_))
    return;
  using std::swap;
  config_value tmp;
  swap(*this, tmp);
  data_ = std::vector<config_value>{std::move(tmp)};
}

void config_value::append(config_value x) {
  convert_to_list();
  get<list>(data_).emplace_back(std::move(x));
}

const char* config_value::type_name() const noexcept {
  return type_name_at_index(data_.index());
}

const char* config_value::type_name_at_index(size_t index) noexcept {
  return type_names[index];
}

bool operator<(const config_value& x, const config_value& y) {
  return x.get_data() < y.get_data();
}

bool operator==(const config_value& x, const config_value& y) {
  return x.get_data() == y.get_data();
}

std::string to_string(const config_value& x) {
  return deep_to_string(x.get_data());
}

std::string get_or(const config_value::dictionary& xs, const std::string& name,
                   const char* default_value) {
  auto result = get_if<std::string>(&xs, name);
  if (result)
    return std::move(*result);
  return default_value;
}

std::string get_or(const dictionary<config_value::dictionary>& xs,
                   string_view name, const char* default_value) {
  auto result = get_if<std::string>(&xs, name);
  if (result)
    return std::move(*result);
  return default_value;
}

std::string get_or(const actor_system_config& cfg, string_view name,
                   const char* default_value) {
  return get_or(content(cfg), name, default_value);
}

void put_impl(config_value::dictionary& dict,
              const std::vector<string_view>& path, config_value& value) {
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
    auto iter = current->emplace(*i, config_value::dictionary{}).first;
    if (auto val = get_if<config_value::dictionary>(&iter->second)) {
      current = val;
    } else {
      iter->second = config_value::dictionary{};
      current = &get<config_value::dictionary>(iter->second);
    }
  }
  // Set key-value pair on the leaf.
  current->insert_or_assign(*back, std::move(value));
}

void put_impl(config_value::dictionary& dict, string_view key,
              config_value& value) {
  std::vector<string_view> path;
  split(path, key, ".");
  put_impl(dict, path, value);
}

void put_impl(dictionary<config_value::dictionary>& dict, string_view key,
              config_value& value) {
  // Split the name into a path.
  std::vector<string_view> path;
  split(path, key, ".");
  // Sanity check. At the very least, we need a category and a key.
  if (path.size() < 2)
    return;
  auto category = path.front();
  path.erase(path.begin());
  put_impl(dict[category], path, value);
}

} // namespace caf

