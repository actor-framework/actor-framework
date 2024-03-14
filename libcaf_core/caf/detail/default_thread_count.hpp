// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <cstddef>

namespace caf::detail {

/// Returns the default number of threads for the scheduler.
CAF_CORE_EXPORT size_t default_thread_count();

} // namespace caf::detail
