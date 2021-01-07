// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"

#include <string>

namespace caf::detail {

CAF_CORE_EXPORT std::string encode_base64(string_view str);

CAF_CORE_EXPORT std::string encode_base64(span<const byte> bytes);

} // namespace caf::detail
