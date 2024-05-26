// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_array.hpp"

namespace caf::detail {
namespace {

const auto empty_array_instance = json::array{};

} // namespace
} // namespace caf::detail

namespace caf {

json_array::json_array() noexcept : arr_(&detail::empty_array_instance) {
  // nop
}

std::string to_string(const json_array& arr) {
  std::string result;
  arr.print_to(result);
  return result;
}

} // namespace caf
