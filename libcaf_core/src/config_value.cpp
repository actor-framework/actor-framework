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

#include "caf/detail/parser/ec.hpp"
#include "caf/detail/parser/read_ini.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/expected.hpp"

namespace caf {

namespace {

const char* type_names[] {
  "integer",
  "boolean",
  "real",
  "atom",
  "timespan",
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

namespace {

struct abstract_consumer {
  virtual ~abstract_consumer() {
    // nop
  }

  template <class T>
  void value(T&& x) {
    value_impl(config_value{std::forward<T>(x)});
  }

  virtual void value_impl(config_value&& x) = 0;
};

struct list_consumer;

struct map_consumer : abstract_consumer {
  using map_type = config_value::dictionary;

  using iterator = map_type::iterator;

  map_consumer begin_map() {
    return {this};
  }

  void end_map() {
    parent->value_impl(config_value{std::move(xs)});
  }

  list_consumer begin_list();

  void key(std::string name) {
    i = xs.emplace_hint(xs.end(), std::make_pair(std::move(name), config_value{}));
  }

  void value_impl(config_value&& x) override {
    i->second = std::move(x);
  }

  map_consumer(abstract_consumer* ptr) : i(xs.end()), parent(ptr) {
    // nop
  }

  map_consumer(map_consumer&& other)
      : xs(std::move(other.xs)),
        i(xs.end()),
        parent(other.parent) {
    // nop
  }

  map_type xs;
  iterator i;
  abstract_consumer* parent;
};

struct list_consumer : abstract_consumer {
  void end_list() {
    parent->value_impl(config_value{std::move(xs)});
  }

  map_consumer begin_map() {
    return {this};
  }

  list_consumer begin_list() {
    return {this};
  }

  void value_impl(config_value&& x) override {
    xs.emplace_back(std::move(x));
  }

  list_consumer(abstract_consumer* ptr) : parent(ptr) {
    // nop
  }

  list_consumer(list_consumer&& other)
      : xs(std::move(other.xs)),
        parent(other.parent) {
    // nop
  }

  config_value::list xs;
  abstract_consumer* parent;
};

list_consumer map_consumer::begin_list() {
  return {this};
}

struct consumer : abstract_consumer {
  config_value result;

  map_consumer begin_map() {
    return {this};
  }

  list_consumer begin_list() {
    return {this};
  }

  void value_impl(config_value&& x) override {
    result = std::move(x);
  }
};

} // namespace <anonymous>

expected<config_value> config_value::parse(std::string::const_iterator first,
                                           std::string::const_iterator last) {
  using namespace detail;
  auto i = first;
  // Sanity check.
  if (i == last)
    return make_error(parser::ec::unexpected_eof);
  // Skip to beginning of the argument.
  while (isspace(*i))
    if (++i == last)
      return make_error(parser::ec::unexpected_eof);
  // Dispatch to parser.
  parser::state<std::string::const_iterator> res;
  consumer f;
  res.i = i;
  res.e = last;
  parser::read_ini_value(res, f);
  if (res.code == detail::parser::ec::success)
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

expected<config_value> config_value::parse(const std::string& str) {
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

} // namespace caf

