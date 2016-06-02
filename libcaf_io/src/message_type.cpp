/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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

namespace caf {
namespace io {
namespace basp {

std::string to_string(message_type x) {
  switch (x) {
    case message_type::server_handshake:
      return "server_handshake";
    case message_type::client_handshake:
      return "client_handshake";
    case message_type::dispatch_message:
      return "dispatch_message";
    case message_type::announce_proxy:
      return "announce_proxy_instance";
    case message_type::kill_proxy:
      return "kill_proxy_instance";
    case message_type::heartbeat:
      return "heartbeat";
    default:
      return "???";
  }
}

} // namespace basp
} // namespace io
} // namespace caf
