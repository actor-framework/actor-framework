// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <cstddef>
#include <source_location>

namespace caf::detail {

[[noreturn]] CAF_CORE_EXPORT void
critical(const char* msg,
         const std::source_location& loc = std::source_location::current(),
         int stack_offset = 0);

} // namespace caf::detail
