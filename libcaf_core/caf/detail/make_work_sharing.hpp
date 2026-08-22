// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <cstddef>
#include <memory>

namespace caf::detail {

CAF_CORE_EXPORT std::unique_ptr<scheduler>
make_work_sharing(actor_system_impl& sys, size_t num_workers,
                  bool system_scheduler);

} // namespace caf::detail
