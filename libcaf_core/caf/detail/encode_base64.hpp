// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_span.hpp"
#include "caf/detail/base64.hpp"
#include "caf/string_view.hpp"

#include <string>

namespace caf::detail {

[[deprecated("use base64::encode instead")]] inline std::string
encode_base64(string_view str) {
  return base64::encode(str);
}

[[deprecated("use base64::encode instead")]] inline std::string
encode_base64(const_byte_span bytes) {
  return base64::encode(bytes);
}

} // namespace caf::detail
