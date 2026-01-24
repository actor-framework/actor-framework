// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/assert.hpp"

#include "caf/detail/critical.hpp"
#include "caf/detail/format.hpp"

namespace caf::detail {

[[noreturn]] void assertion_failed(const char* stmt,
                                   const std::source_location& loc) {
  auto errmsg = format("assertion '{}' failed", stmt);
  critical(errmsg.c_str(), loc, 1);
}

} // namespace caf::detail
