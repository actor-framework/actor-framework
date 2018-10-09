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

#include "caf/ip_endpoint.hpp"

namespace caf {

ip_endpoint::ip_endpoint() : port_(0), transport_(protocol::tcp) {
  // nop
}

ip_endpoint::ip_endpoint(ip_address addr, uint16_t port, protocol::transport tp)
    : address_(addr),
      port_(port),
      transport_(tp) {
  // nop
}

ip_endpoint::ip_endpoint(ipv4_address addr, uint16_t port,
                         protocol::transport tp)
    : address_(addr),
      port_(port),
      transport_(tp) {
  // nop
}

int ip_endpoint::compare(const ip_endpoint& other) const noexcept {
  auto sub_res = address_.compare(other.address());
  auto compress = [](const ip_endpoint& x) {
    return (x.port_ << 1) | x.transport_;
  };
  return sub_res != 0 ? sub_res : compress(*this) - compress(other);
}

} // namespace caf
