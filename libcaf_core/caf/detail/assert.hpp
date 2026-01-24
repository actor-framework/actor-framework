// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <source_location>

namespace caf::detail {

[[noreturn]] CAF_CORE_EXPORT void
assertion_failed(const char* stmt, const std::source_location& loc
                                   = std::source_location::current());

} // namespace caf::detail

#ifdef CAF_ENABLE_RUNTIME_CHECKS
#  define CAF_ASSERT(stmt)                                                     \
    if (static_cast<bool>(stmt) == false) {                                    \
      ::caf::detail::assertion_failed(#stmt);                                  \
    }                                                                          \
    static_cast<void>(0)
#else
#  define CAF_ASSERT(unused) static_cast<void>(0)
#endif
