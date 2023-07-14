// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/raise_error.hpp"

#include "caf/logger.hpp"

namespace caf::detail {

void log_cstring_error(const char* cstring) {
  CAF_IGNORE_UNUSED(cstring);
  CAF_LOG_ERROR(cstring);
}

} // namespace caf::detail
