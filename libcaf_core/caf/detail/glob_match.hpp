// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::detail {

// gitignore-style pathname globbing.
bool glob_match(const char* str, const char* glob);

} // namespace caf::detail
