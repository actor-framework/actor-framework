// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/network/stream.hpp"

namespace caf::io::network {

/// A concrete stream with a technology-dependent policy for sending and
/// receiving data from a socket.
template <class ProtocolPolicy>
class stream_impl : public stream {
public:
  template <class... Ts>
  stream_impl(default_multiplexer& mpx, native_socket sockfd, Ts&&... xs)
    : stream(mpx, sockfd), policy_(std::forward<Ts>(xs)...) {
    // nop
  }

  void handle_event(io::network::operation op) override {
    this->handle_event_impl(op, policy_);
  }

private:
  ProtocolPolicy policy_;
};

} // namespace caf::io::network
