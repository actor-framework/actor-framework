// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

namespace caf::detail {

[[noreturn]] CAF_CORE_EXPORT void critical(const char* file, int line,
                                           const char* msg);

} // namespace caf::detail

#define CAF_CRITICAL(msg) ::caf::detail::critical(__FILE__, __LINE__, msg)
