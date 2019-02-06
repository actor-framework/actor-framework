/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/basp/message_type.hpp"

#include "caf/detail/enum_to_string.hpp"

namespace caf {
namespace io {
namespace basp {

namespace {

const char* message_type_strings[] = {
  "server_handshake",
  "client_handshake",
  "direct_message",
  "routed_message",
  "proxy_creation",
  "proxy_destruction",
  "heartbeat"
};

} // namespace <anonymous>

std::string to_string(message_type x) {
  return detail::enum_to_string(x, message_type_strings);
}

} // namespace basp
} // namespace io
} // namespace caf
