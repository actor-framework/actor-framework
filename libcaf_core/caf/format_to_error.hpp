// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/format.hpp"
#include "caf/error.hpp"

namespace caf {

/// Formats the given arguments into a string and returns an error with the
/// specified code and the formatted string as message.
template <class Enum, class... Args>
std::enable_if_t<is_error_code_enum_v<Enum>, error>
format_to_error(Enum code, std::string_view fstr, Args&&... args) {
  return error{code, detail::format(fstr, std::forward<Args>(args)...)};
}

} // namespace caf
