// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/critical.hpp"
#include "caf/detail/format.hpp"
#include "caf/format_string_with_location.hpp"

namespace caf::detail {

/// Like `critical`, but uses the format API instead of just accepting a string.
template <class... Args>
[[noreturn]] void panic(caf::format_string_with_location fstr, Args&&... args) {
  auto errmsg = format(fstr.value, std::forward<Args>(args)...);
  critical(errmsg.c_str(), fstr.location, 1);
}

} // namespace caf::detail
