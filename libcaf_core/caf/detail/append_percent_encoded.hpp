// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

// Escapes all reserved characters according to RFC 3986 in `x` and
// adds the encoded string to `str`.
CAF_CORE_EXPORT void append_percent_encoded(std::string& str, string_view x,
                                            bool is_path = false);

} // namespace caf::detail
