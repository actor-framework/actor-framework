// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/config_option.hpp"

#include "caf/config.hpp"
#include "caf/config_value.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"

#include <algorithm>
#include <limits>
#include <numeric>

using std::string;

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

config_option::config_option(std::string_view category, std::string_view name,
                             std::string_view description,
                             const meta_state* meta, void* value)
  : meta_(meta), value_(value) {
  using std::accumulate;
  using std::copy;
  auto comma = name.find(',');
  auto long_name = name.substr(0, comma);
  auto short_names = comma == std::string_view::npos ? std::string_view{}
                                                     : name.substr(comma + 1);
  auto total_size = [](std::initializer_list<std::string_view> xs) {
    return (xs.size() - 1) // one separator between all fields
           + accumulate(xs.begin(), xs.end(), size_t{0},
                        [](size_t x, std::string_view sv) {
                          return x + sv.size();
                        });
  };
  auto ts = total_size({category, long_name, short_names, description});
  CAF_ASSERT(ts <= std::numeric_limits<uint16_t>::max());
  buf_size_ = static_cast<uint16_t>(ts);
  buf_.reset(new char[ts]);
  // fill the buffer with "<category>.<long-name>,<short-name>,<descriptions>"
  auto first = buf_.get();
  auto i = first;
  auto pos = [&] { return static_cast<uint16_t>(std::distance(first, i)); };
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

config_option::config_option(const config_option& other)
  : category_separator_{other.category_separator_},
    long_name_separator_{other.long_name_separator_},
    short_names_separator_{other.short_names_separator_},
    buf_size_{other.buf_size_},
    meta_{other.meta_},
    value_{other.value_} {
  buf_.reset(new char[buf_size_]);
  std::copy_n(other.buf_.get(), buf_size_, buf_.get());
}

config_option& config_option::operator=(const config_option& other) {
  config_option tmp{other};
  swap(*this, tmp);
  return *this;
}

void swap(config_option& first, config_option& second) noexcept {
  using std::swap;
  swap(first.buf_, second.buf_);
  swap(first.category_separator_, second.category_separator_);
  swap(first.long_name_separator_, second.long_name_separator_);
  swap(first.short_names_separator_, second.short_names_separator_);
  swap(first.buf_size_, second.buf_size_);
  swap(first.meta_, second.meta_);
  swap(first.value_, second.value_);
}

// -- properties ---------------------------------------------------------------

std::string_view config_option::category() const noexcept {
  return buf_slice(buf_[0] == '?' ? 1 : 0, category_separator_);
}

std::string_view config_option::long_name() const noexcept {
  return buf_slice(category_separator_ + 1, long_name_separator_);
}

std::string_view config_option::short_names() const noexcept {
  return buf_slice(long_name_separator_ + 1, short_names_separator_);
}

std::string_view config_option::description() const noexcept {
  return buf_slice(short_names_separator_ + 1, buf_size_);
}

std::string_view config_option::full_name() const noexcept {
  return buf_slice(buf_[0] == '?' ? 1 : 0, long_name_separator_);
}

error config_option::sync(config_value& x) const {
  return meta_->sync(value_, x);
}

std::string_view config_option::type_name() const noexcept {
  return meta_->type_name;
}

bool config_option::is_flag() const noexcept {
  return type_name() == "bool";
}

bool config_option::has_flat_cli_name() const noexcept {
  return buf_[0] == '?' || category() == "global";
}

std::string_view config_option::buf_slice(size_t from,
                                          size_t to) const noexcept {
  CAF_ASSERT(from <= to);
  return {buf_.get() + from, to - from};
}

// TODO: consider using `config_option_set` and deprecating this
config_option::find_result config_option::find_by_long_name(
  config_option::argument_iterator first,
  config_option::argument_iterator last) const noexcept {
  auto argument_name = long_name();
  for (; first != last; ++first) {
    std::string_view str{*first};
    // Make sure this is a long option starting with "--".
    if (!starts_with(str, "--"))
      continue;
    str.remove_prefix(2);
    // Make sure we are dealing with the right key.
    if (!starts_with(str, argument_name))
      continue;
    str.remove_prefix(argument_name.size());
    // check for flag
    if (is_flag() && str.empty()) {
      return {first, first + 1, str};
    } else if (starts_with(str, "=")) {
      // Remove leading '=' and return the value.
      str.remove_prefix(1);
      return {first, first + 1, str};
    } else if (auto val = first + 1; str.empty() && val != last) {
      // Get the next argument the value
      return {first, first + 2, std::string_view{*val}};
    } else {
      continue;
    }
  }
  return {first, first, std::string_view{}};
}

} // namespace caf
