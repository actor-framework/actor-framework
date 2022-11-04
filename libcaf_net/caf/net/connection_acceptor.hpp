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

  using read_result = typename socket_manager::read_result;

  using write_result = typename socket_manager::write_result;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit connection_acceptor(size_t limit, Ts&&... xs)
    : factory_(std::forward<Ts>(xs)...), limit_(limit) {
    // nop
  }

  // -- interface functions ----------------------------------------------------

  template <class LowerLayerPtr>
  error init(socket_manager* owner, LowerLayerPtr down, const settings& cfg) {
    CAF_LOG_TRACE("");
    owner_ = owner;
    cfg_ = cfg;
    if (auto err = factory_.init(owner, cfg))
      return err;
    down->register_reading();
    return none;
  }

  template <class LowerLayerPtr>
  read_result handle_read_event(LowerLayerPtr down) {
    CAF_LOG_TRACE("");
    if (auto x = accept(down->handle())) {
      socket_manager_ptr child = factory_.make(*x, owner_->mpx_ptr());
      if (!child) {
        CAF_LOG_ERROR("factory failed to create a new child");
        down->abort_reason(sec::runtime_error);
        return read_result::stop;
      }
      if (auto err = child->init(cfg_)) {
        CAF_LOG_ERROR("failed to initialize new child:" << err);
        down->abort_reason(std::move(err));
        return read_result::stop;
      }
      if (limit_ == 0) {
        return read_result::again;
      } else {
        return ++accepted_ < limit_ ? read_result::again : read_result::stop;
      }
    } else {
      CAF_LOG_ERROR("accept failed:" << x.error());
      return read_result::stop;
    }
  }

  template <class LowerLayerPtr>
  static read_result handle_buffered_data(LowerLayerPtr) {
    return read_result::again;
  }

  template <class LowerLayerPtr>
  static read_result handle_continue_reading(LowerLayerPtr) {
    return read_result::again;
  }

  template <class LowerLayerPtr>
  write_result handle_write_event(LowerLayerPtr) {
    CAF_LOG_ERROR("connection_acceptor received write event");
    return write_result::stop;
  }

  template <class LowerLayerPtr>
  static write_result handle_continue_writing(LowerLayerPtr) {
    CAF_LOG_ERROR("connection_acceptor received continue writing event");
    return write_result::stop;
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr, const error& reason) {
    CAF_LOG_ERROR("connection_acceptor aborts due to an error: " << reason);
    factory_.abort(reason);
  }

private:
  factory_type factory_;

  socket_manager* owner_;

  size_t limit_;

  size_t accepted_ = 0;

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
