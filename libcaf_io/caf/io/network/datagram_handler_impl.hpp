// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/fwd.hpp"
#include "caf/io/network/datagram_handler.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/operation.hpp"

namespace caf::io::network {

/// A concrete datagram_handler with a technology-dependent policy.
template <class ProtocolPolicy>
class datagram_handler_impl : public datagram_handler {
public:
  template <class... Ts>
  datagram_handler_impl(default_multiplexer& mpx, native_socket sockfd,
                        Ts&&... xs)
    : datagram_handler(mpx, sockfd), policy_(std::forward<Ts>(xs)...) {
    // nop
  }

  void handle_event(io::network::operation op) override {
    this->handle_event_impl(op, policy_);
  }

private:
  ProtocolPolicy policy_;
};

} // namespace caf::io::network
