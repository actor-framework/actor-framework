/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

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
