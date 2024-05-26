// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/fwd.hpp"
#include "caf/io/network/acceptor.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/operation.hpp"

namespace caf::io::network {

/// A concrete acceptor with a technology-dependent policy.
template <class ProtocolPolicy>
class acceptor_impl : public acceptor {
public:
  template <class... Ts>
  acceptor_impl(default_multiplexer& mpx, native_socket sockfd, Ts&&... xs)
    : acceptor(mpx, sockfd), policy_(std::forward<Ts>(xs)...) {
    // nop
  }

  void handle_event(io::network::operation op) override {
    this->handle_event_impl(op, policy_);
  }

private:
  ProtocolPolicy policy_;
};

} // namespace caf::io::network
