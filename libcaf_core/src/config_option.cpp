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
#include "caf/config_value.hpp"
#include "caf/error.hpp"
#include "caf/optional.hpp"

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
                             string description, const meta_state* meta,
                             void* value)
    : category_(move(category)),
      long_name_(get_long_name(name)),
      short_names_(get_short_names(name)),
      description_(move(description)),
      meta_(meta),
      value_(value) {
  CAF_ASSERT(meta_ != nullptr);
}

string config_option::full_name() const {
  std::string result = category();
  result += '.';
  result += long_name_;
  return result;
}

error config_option::check(const config_value& x) const {
  CAF_ASSERT(meta_->check != nullptr);
  return meta_->check(x);
}

void config_option::store(const config_value& x) const {
  if (value_ != nullptr) {
    CAF_ASSERT(meta_->store != nullptr);
    meta_->store(value_, x);
  }
}

const std::string& config_option::type_name() const noexcept {
  return meta_->type_name;
}

bool config_option::is_flag() const noexcept {
  return type_name() == "boolean";
}

optional<config_value> config_option::get() const {
  if (value_ != nullptr && meta_->get != nullptr)
    return meta_->get(value_);
  return none;
}

} // namespace caf
