/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include "caf/config_option.hpp"

#include <iostream>

namespace caf {

config_option::config_option(const char* cat, const char* nm, const char* expl)
    : category_(cat),
      name_(nm),
      explanation_(expl),
      short_name_('\0') {
  auto last = name_.end();
  auto comma = std::find(name_.begin(), last, ',');
  if (comma != last) {
    auto i = comma;
    ++i;
    if (i != last)
      short_name_ = *i;
    name_.erase(comma, last);
  }
}

config_option::~config_option() {
  // nop
}

std::string config_option::full_name() const {
  std::string res = category();
  res += '.';
  auto name_begin = name();
  const char* name_end = strchr(name(), ',');
  if (name_end)
    res.insert(res.end(), name_begin, name_end);
  else
    res += name();
  return res;
}

const char* config_option::type_name_visitor::operator()(const std::string&) const {
  return "a string";
}
const char* config_option::type_name_visitor::operator()(double) const {
  return "a double";
}
const char* config_option::type_name_visitor::operator()(int64_t) const {
  return "an integer";
}
const char* config_option::type_name_visitor::operator()(size_t) const {
  return "an unsigned integer";
}
const char* config_option::type_name_visitor::operator()(uint16_t) const {
  return "an unsigned short integer";
}
const char* config_option::type_name_visitor::operator()(bool) const {
  return "a boolean";
}
const char* config_option::type_name_visitor::operator()(atom_value) const {
  return "an atom";
}

bool config_option::assign_config_value(size_t& x, int64_t& y) {
  if (y < 0 || ! unsigned_assign_in_range(x, y))
    return false;
  x = static_cast<size_t>(y);
  return true;
}

bool config_option::assign_config_value(uint16_t& x, int64_t& y) {
  if (y < 0 || y > std::numeric_limits<uint16_t>::max())
    return false;
  x = static_cast<uint16_t>(y);
  return true;
}

void config_option::report_type_error(size_t ln, config_value& x,
                                      const char* expected) {
  type_name_visitor tnv;
  std::cerr << "error in line " << ln << ": expected "
            << expected << " found "
            << apply_visitor(tnv, x) << std::endl;
}

} // namespace caf
