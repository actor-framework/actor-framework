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

config_value::list& config_value::as_list() {
  convert_to_list();
  return get<list>(*this);
}

config_value::dictionary& config_value::as_dictionary() {
  if (!holds_alternative<dictionary>(*this))
    *this = dictionary{};
  return get<dictionary>(*this);
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

} // namespace caf

