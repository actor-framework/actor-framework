// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/string_view.hpp"
#include "caf/unordered_flat_map.hpp"

namespace caf::net::http {

/// Convenience type alias for a key-value map storing header fields.
using header_fields_map = unordered_flat_map<string_view, string_view>;

} // namespace caf::net::http
