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

#include <algorithm>
#include <limits>
#include <numeric>

#include "caf/config.hpp"
#include "caf/config_value.hpp"
#include "caf/error.hpp"
#include "caf/optional.hpp"

using std::move;
using std::string;

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

config_option::config_option(string_view category, string_view name,
                             string_view description, const meta_state* meta,
                             void* value)
    : meta_(meta),
      value_(value) {
  using std::copy;
  using std::accumulate;
  auto comma = name.find(',');
  auto long_name = name.substr(0, comma);
  auto short_names = comma == string_view::npos ? string_view{}
                                                : name.substr(comma + 1);
  auto total_size = [](std::initializer_list<string_view> xs) {
    return (xs.size() - 1) // one separator between all fields
           + accumulate(xs.begin(), xs.end(), size_t{0},
                        [](size_t x, string_view sv) { return x + sv.size(); });
  };
  auto ts = total_size({category, long_name, short_names, description});
  CAF_ASSERT(ts <= std::numeric_limits<uint16_t>::max());
  buf_size_ = static_cast<uint16_t>(ts);
  buf_.reset(new char[ts]);
  // fille the buffer with "<category>.<long-name>,<short-name>,<descriptions>"
  auto first = buf_.get();
  auto i = first;
  auto pos = [&] {
    return static_cast<uint16_t>(std::distance(first, i));
  };
  // <category>.
  i = copy(category.begin(), category.end(), i);
  category_separator_ = pos();
  *i++ = '.';
  // <long-name>,
  i = copy(long_name.begin(), long_name.end(), i);
  long_name_separator_ = pos();
  *i++ = ',';
  // <short-names>,
  i = copy(short_names.begin(), short_names.end(), i);
  short_names_separator_ = pos();
  *i++ = ',';
  // <description>
  i = copy(description.begin(), description.end(), i);
  CAF_ASSERT(pos() == buf_size_);
}

// -- properties ---------------------------------------------------------------

string_view config_option::category() const noexcept {
  return buf_slice(0, category_separator_);
}

string_view config_option::long_name() const noexcept {
  return buf_slice(category_separator_ + 1, long_name_separator_);
}

string_view config_option::short_names() const noexcept {
  return buf_slice(long_name_separator_ + 1, short_names_separator_);
}

string_view config_option::description() const noexcept {
  return buf_slice(short_names_separator_ + 1, buf_size_);
}

string_view config_option::full_name() const noexcept {
  return buf_slice(0, long_name_separator_);
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

string_view config_option::type_name() const noexcept {
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

string_view config_option::buf_slice(size_t from, size_t to) const noexcept {
  CAF_ASSERT(from <= to);
  return {buf_.get() + from, to - from};
}

} // namespace caf
