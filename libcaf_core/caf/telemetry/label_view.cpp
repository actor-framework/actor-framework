// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/telemetry/label_view.hpp"

#include "caf/telemetry/label.hpp"

namespace caf::telemetry {

int label_view::compare(const label_view& other) const noexcept {
  return label::compare(*this, other);
}

int label_view::compare(const label& other) const noexcept {
  return label::compare(*this, other);
}

std::string to_string(const label_view& x) {
  std::string result;
  result.reserve(x.name().size() + 1 + x.value().size());
  result.insert(result.end(), x.name().begin(), x.name().end());
  result += '=';
  result.insert(result.end(), x.value().begin(), x.value().end());
  return result;
}

} // namespace caf::telemetry
