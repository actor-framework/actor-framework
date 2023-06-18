// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/telemetry/label.hpp"

#include "caf/telemetry/label_view.hpp"

namespace caf::telemetry {

label::label(std::string_view name, std::string_view value)
  : name_length_(name.size()) {
  str_.reserve(name.size() + value.size() + 1);
  str_.insert(str_.end(), name.begin(), name.end());
  str_ += '=';
  str_.insert(str_.end(), value.begin(), value.end());
}

label::label(const label_view& view) : label(view.name(), view.value()) {
  // nop
}

void label::value(std::string_view new_value) {
  str_.erase(name_length_ + 1);
  str_.insert(str_.end(), new_value.begin(), new_value.end());
}

int label::compare(const label_view& x) const noexcept {
  return compare(*this, x);
}

int label::compare(const label& x) const noexcept {
  return compare(*this, x);
}

std::string to_string(const label& x) {
  return x.str();
}

} // namespace caf::telemetry
