// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/timestamp.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/parse.hpp"
#include "caf/expected.hpp"

namespace caf {

timestamp make_timestamp() {
  return std::chrono::system_clock::now();
}

std::string timestamp_to_string(timestamp x) {
  return deep_to_string(x.time_since_epoch().count());
}

expected<timestamp> timestamp_from_string(string_view str) {
  timestamp result;
  if (auto err = detail::parse(str, result); !err)
    return result;
  else
    return err;
}

void append_timestamp_to_string(std::string& x, timestamp y) {
  x += timestamp_to_string(y);
}

} // namespace caf
