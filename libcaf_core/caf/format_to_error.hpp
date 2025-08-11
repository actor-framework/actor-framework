// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/format.hpp"
#include "caf/error.hpp"
#include "caf/is_error_code_enum.hpp"

namespace caf {

/// Formats the given arguments into a string and returns an error with the
/// specified code and the formatted string as message.
template <error_code_enum Enum, class... Args>
error format_to_error(Enum code, std::string_view fstr, Args&&... args) {
  return error{code, detail::format(fstr, std::forward<Args>(args)...)};
}

} // namespace caf
