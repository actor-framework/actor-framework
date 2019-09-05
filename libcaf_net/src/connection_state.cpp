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

#include "caf/net/basp/connection_state.hpp"

namespace caf {
namespace net {
namespace basp {

namespace {

const char* connection_state_names[] = {
  "await_magic_number",
  "await_handshake_header",
  "await_handshake_payload",
  "await_header",
  "await_payload",
  "shutdown",
};

} // namespace

std::string to_string(connection_state x) {
  return connection_state_names[static_cast<uint8_t>(x)];
}

} // namespace basp
} // namespace net
} // namespace caf
