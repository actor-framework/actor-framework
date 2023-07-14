// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/json_array.hpp"

namespace caf {

std::string to_string(const json_array& arr) {
  std::string result;
  arr.print_to(result);
  return result;
}

} // namespace caf
