// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

namespace caf::detail {

/// Sets the name thread shown by the OS. Not supported on all platforms
/// (no-op on Windows).
CAF_CORE_EXPORT void set_thread_name(const char* name);

} // namespace caf::detail
