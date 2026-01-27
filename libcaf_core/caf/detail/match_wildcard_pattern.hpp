// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <string_view>

namespace caf::detail {

/// Matches a pattern with '*' (zero or more characters) and '?' (exactly one
/// character) wildcards against the input.
/// @returns `true` if the input matches the pattern, `false` otherwise.
CAF_CORE_EXPORT bool match_wildcard_pattern(std::string_view input,
                                            std::string_view pattern);

} // namespace caf::detail
