// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/logger.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/send.hpp"

namespace caf::net {

/// A connection_acceptor accepts connections from an accept socket and creates
/// socket managers to handle them via its factory.
template <class Socket, class Factory>
class connection_acceptor {
public:
  // -- member types -----------------------------------------------------------

  using input_tag = tag::io_event_oriented;

  using socket_type = Socket;

  using factory_type = Factory;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit connection_acceptor(Ts&&... xs) : factory_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- member functions -------------------------------------------------------

  template <class LowerLayerPtr>
  error init(socket_manager* owner, LowerLayerPtr parent, const settings& config) {
    CAF_LOG_TRACE("");
    owner_ = owner;
    cfg_ = config;
    if (auto err = factory_.init(owner, config))
      return err;
    parent->register_reading();
    return none;
  }

  template <class LowerLayerPtr>
  bool handle_read_event(LowerLayerPtr parent) {
    CAF_LOG_TRACE("");
    if (auto x = accept(parent->handle())) {
      socket_manager_ptr child = factory_.make(*x, owner_->mpx_ptr());
      CAF_ASSERT(child != nullptr);
      if (auto err = child->init(cfg_)) {
        CAF_LOG_ERROR("failed to initialize new child:" << err);
        parent->abort_reason(std::move(err));
        return false;
      }
      return true;
    } else {
      CAF_LOG_ERROR("accept failed:" << x.error());
      return false;
    }
  }

  template <class LowerLayerPtr>
  static void continue_reading(LowerLayerPtr) {
    // nop
  }

  template <class LowerLayerPtr>
  bool handle_write_event(LowerLayerPtr) {
    CAF_LOG_ERROR("connection_acceptor received write event");
    return false;
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr, const error& reason) {
    CAF_LOG_ERROR("connection_acceptor aborts due to an error: " << reason);
    factory_.abort(reason);
  }

private:
  factory_type factory_;

  socket_manager* owner_;

  settings cfg_;
};

/// Converts a function object into a factory object for a
/// @ref connection_acceptor.
template <class FunctionObject>
class connection_acceptor_factory_adapter {
public:
  explicit connection_acceptor_factory_adapter(FunctionObject f)
    : f_(std::move(f)) {
    // nop
  }

  connection_acceptor_factory_adapter(connection_acceptor_factory_adapter&&)
    = default;

  error init(socket_manager*, const settings&) {
    return none;
  }

  template <class Socket>
  socket_manager_ptr make(Socket connected_socket, multiplexer* mpx) {
    return f_(std::move(connected_socket), mpx);
  }

  void abort(const error&) {
    // nop
  }

private:
  FunctionObject f_;
};

} // namespace caf::net
