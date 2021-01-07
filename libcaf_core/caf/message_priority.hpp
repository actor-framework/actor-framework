// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

#include "caf/detail/core_export.hpp"

namespace caf {

enum class message_priority {
  high = 0,
  normal = 1,
};

using high_message_priority_constant
  = std::integral_constant<message_priority, message_priority::high>;

using normal_message_priority_constant
  = std::integral_constant<message_priority, message_priority::normal>;

CAF_CORE_EXPORT std::string to_string(message_priority);

} // namespace caf
