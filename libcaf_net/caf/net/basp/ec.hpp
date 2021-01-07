// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <string>

#include "caf/detail/net_export.hpp"
#include "caf/is_error_code_enum.hpp"

namespace caf::net::basp {

/// BASP-specific error codes.
enum class ec : uint8_t {
  invalid_magic_number = 1,
  unexpected_number_of_bytes,
  unexpected_payload,
  missing_payload,
  illegal_state,
  invalid_handshake,
  missing_handshake,
  unexpected_handshake,
  version_mismatch,
  unimplemented = 10,
  app_identifiers_mismatch,
  invalid_payload,
  invalid_scheme,
  invalid_locator,
};

/// @relates ec
CAF_NET_EXPORT std::string to_string(ec x);

} // namespace caf::net::basp

CAF_ERROR_CODE_ENUM(caf::net::basp::ec)
