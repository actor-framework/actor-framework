// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/network/protocol.hpp"

namespace caf::io::network {

std::string to_string(const protocol& x) {
  std::string result;
  result += to_string(x.trans);
  result += "/";
  result += to_string(x.net);
  return result;
}

} // namespace caf::io::network
