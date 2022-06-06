// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/connection_factory.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/settings.hpp"

namespace caf::net {

/// A connection_acceptor accepts connections from an accept socket and creates
/// socket managers to handle them via its factory.
template <class Socket>
class connection_acceptor : public socket_event_layer {
public:
  // -- member types -----------------------------------------------------------

  using socket_type = Socket;

  using connected_socket_type = typename socket_type::connected_socket_type;

  using factory_type = connection_factory<connected_socket_type>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class FactoryPtr, class... Ts>
  connection_acceptor(Socket fd, FactoryPtr fptr, size_t max_connections)
    : fd_(fd),
      factory_(factory_type::decorate(std::move(fptr))),
      max_connections_(max_connections) {
    CAF_ASSERT(max_connections_ > 0);
  }

  ~connection_acceptor() {
    on_conn_close_.dispose();
    if (fd_.id != invalid_socket_id)
      close(fd_);
  }

  // -- factories --------------------------------------------------------------

  template <class FactoryPtr, class... Ts>
  static std::unique_ptr<connection_acceptor>
  make(Socket fd, FactoryPtr fptr, size_t max_connections) {
    return std::make_unique<connection_acceptor>(fd, std::move(fptr),
                                                 max_connections);
  }

  // -- implementation of socket_event_layer -----------------------------------

  error start(socket_manager* owner, const settings& cfg) override {
    CAF_LOG_TRACE("");
    owner_ = owner;
    cfg_ = cfg;
    if (auto err = factory_->start(owner, cfg)) {
      CAF_LOG_DEBUG("factory_->start failed:" << err);
      return err;
    }
    on_conn_close_ = make_action([this] { connection_closed(); });
    owner->register_reading();
    return none;
  }

  socket handle() const override {
    return fd_;
  }

  void handle_read_event() override {
    CAF_LOG_TRACE("");
    CAF_ASSERT(owner_ != nullptr);
    if (open_connections_ == max_connections_) {
      owner_->deregister_reading();
    } else if (auto x = accept(fd_)) {
      socket_manager_ptr child = factory_->make(owner_->mpx_ptr(), *x);
      if (!child) {
        CAF_LOG_ERROR("factory failed to create a new child");
        on_conn_close_.dispose();
        owner_->shutdown();
        return;
      }
      if (++open_connections_ == max_connections_)
        owner_->deregister_reading();
      child->add_cleanup_listener(on_conn_close_);
      std::ignore = child->start(cfg_);
    } else if (x.error() == sec::unavailable_or_would_block) {
      // Encountered a "soft" error: simply try again later.
      CAF_LOG_DEBUG("accept failed:" << x.error());
    } else {
      // Encountered a "hard" error: stop.
      abort(x.error());
      owner_->deregister_reading();
    }
  }

  void handle_write_event() override {
    CAF_LOG_ERROR("connection_acceptor received write event");
    owner_->deregister_writing();
  }

  void abort(const error& reason) override {
    CAF_LOG_ERROR("connection_acceptor aborts due to an error:" << reason);
    factory_->abort(reason);
    on_conn_close_.dispose();
  }

private:
  void connection_closed() {
    if (open_connections_-- == max_connections_)
      owner_->register_reading();
  }

  Socket fd_;

  connection_factory_ptr<connected_socket_type> factory_;

  size_t max_connections_;

  size_t open_connections_ = 0;

  socket_manager* owner_ = nullptr;

  action on_conn_close_;

  settings cfg_;
};

} // namespace caf::net
