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

#include "caf/config_option.hpp"

#include <iostream>

#include "caf/config.hpp"
#include "caf/error.hpp"

using std::move;
using std::string;

namespace {

string get_long_name(const char* name) {
  auto i= strchr(name, ',');
  return i != nullptr ? string{name, i} : string{name};
}

string get_short_names(const char* name) {
  auto substr = strchr(name, ',');
  if (substr != nullptr)
    return ++substr;
  return string{};
}

} // namespace <anonymous>

namespace caf {

config_option::config_option(string category, const char* name,
                             string description, bool is_flag,
                             const vtbl_type& vtbl, void* value)
    : category_(move(category)),
      long_name_(get_long_name(name)),
      short_names_(get_short_names(name)),
      description_(move(description)),
      is_flag_(is_flag),
      vtbl_(vtbl),
      value_(value) {
  // nop
}

string config_option::full_name() const {
  std::string result = category();
  result += '.';
  result += long_name_;
  return result;
}

error config_option::check(const config_value& x) const {
  CAF_ASSERT(vtbl_.check != nullptr);
  return vtbl_.check(x);
}

void config_option::store(const config_value& x) const {
  if (value_ != nullptr) {
    CAF_ASSERT(vtbl_.store != nullptr);
    vtbl_.store(value_, x);
  }
}

std::string config_option::type_name() const {
  CAF_ASSERT(vtbl_.type_name != nullptr);
  return vtbl_.type_name();
}

} // namespace caf
