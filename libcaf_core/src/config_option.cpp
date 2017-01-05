/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

const char* type_name_visitor_tbl[] {
  "a boolean", 
  "a float", 
  "a double",
  "a string",
  "an atom_value",
  "an 8-bit integer",
  "an 8-bit unsigned integer",
  "a 16-bit integer",
  "a 16-bit unsigned integer",
  "a 32-bit integer",
  "a 32-bit unsigned integer",
  "a 64-bit integer",
  "a 64-bit unsigned integer"
};

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
  if (name_end != nullptr)
    res.insert(res.end(), name_begin, name_end);
  else
    res += name();
  return res;
}

void config_option::report_type_error(size_t ln, config_value& x,
                                      const char* expected,
                                      optional<std::ostream&> out) {
  if (!out)
    return;
  type_name_visitor tnv;
  *out << "error in line " << ln << ": expected "
       << expected << " found "
       << apply_visitor(tnv, x) << '\n';
}

} // namespace caf
