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

#ifndef CAF_IO_NETWORK_PROTOCOL_HPP
#define CAF_IO_NETWORK_PROTOCOL_HPP

#include <cstddef>

namespace caf {
namespace io {
namespace network {

enum class protocol : uint32_t {
  ethernet,
  ipv4,
  ipv6
};

constexpr const char* to_string(protocol value) {
  return value == protocol::ethernet ? "ethernet"
                                     : (value == protocol::ipv4 ? "ipv4"
                                                                : "ipv6");
}

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_PROTOCOL_HPP
