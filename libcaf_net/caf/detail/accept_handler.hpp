// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/connection_factory.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/settings.hpp"

namespace caf::detail {

/// Accepts incoming clients with an Acceptor and handles them via a connection
/// factory.
template <class Acceptor, class ConnectionHandle>
class accept_handler : public net::socket_event_layer {
public:
  // -- member types -----------------------------------------------------------

  using socket_type = net::socket;

  using connection_handle = ConnectionHandle;

  using factory_type = connection_factory<connection_handle>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class FactoryPtr, class... Ts>
  accept_handler(Acceptor acc, FactoryPtr fptr, size_t max_connections)
    : acc_(std::move(acc)),
      factory_(std::move(fptr)),
      max_connections_(max_connections) {
    CAF_ASSERT(max_connections_ > 0);
  }

  ~accept_handler() {
    on_conn_close_.dispose();
    if (valid(acc_))
      close(acc_);
  }

  // -- factories --------------------------------------------------------------

  template <class FactoryPtr, class... Ts>
  static std::unique_ptr<accept_handler>
  make(Acceptor acc, FactoryPtr fptr, size_t max_connections) {
    return std::make_unique<accept_handler>(std::move(acc), std::move(fptr),
                                            max_connections);
  }

  // -- implementation of socket_event_layer -----------------------------------

  error start(net::socket_manager* owner) override {
    CAF_LOG_TRACE("");
    owner_ = owner;
    if (auto err = factory_->start(owner)) {
      CAF_LOG_DEBUG("factory_->start failed:" << err);
      return err;
    }
    on_conn_close_ = make_action([this] { connection_closed(); });
    owner->register_reading();
    return none;
  }

  net::socket handle() const override {
    return acc_.fd();
  }

  void handle_read_event() override {
    CAF_LOG_TRACE("");
    CAF_ASSERT(owner_ != nullptr);
    if (open_connections_ == max_connections_) {
      owner_->deregister_reading();
    } else if (auto conn = accept(acc_)) {
      auto child = factory_->make(owner_->mpx_ptr(), std::move(*conn));
      if (!child) {
        CAF_LOG_ERROR("factory failed to create a new child");
        on_conn_close_.dispose();
        owner_->shutdown();
        return;
      }
      if (++open_connections_ == max_connections_)
        owner_->deregister_reading();
      child->add_cleanup_listener(on_conn_close_);
      std::ignore = child->start();
    } else if (conn.error() == sec::unavailable_or_would_block) {
      // Encountered a "soft" error: simply try again later.
      CAF_LOG_DEBUG("accept failed:" << conn.error());
    } else {
      // Encountered a "hard" error: stop.
      abort(conn.error());
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
    self_ref_ = nullptr;
  }

  void self_ref(disposable ref) {
    self_ref_ = std::move(ref);
  }

private:
  void connection_closed() {
    if (open_connections_ == max_connections_)
      owner_->register_reading();
    --open_connections_;
  }

  Acceptor acc_;

  detail::connection_factory_ptr<connection_handle> factory_;

  size_t max_connections_;

  size_t open_connections_ = 0;

  net::socket_manager* owner_ = nullptr;

  action on_conn_close_;

  /// Type-erased handle to the @ref socket_manager. This reference is important
  /// to keep the acceptor alive while the manager is not registered for writing
  /// or reading.
  disposable self_ref_;
};

} // namespace caf::detail
