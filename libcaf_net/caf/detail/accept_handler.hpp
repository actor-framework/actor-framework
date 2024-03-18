// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/async/execution_context.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/log/net.hpp"
#include "caf/settings.hpp"

namespace caf::detail {

/// Accepts incoming clients with an Acceptor and handles them via a connection
/// factory.
template <class Acceptor>
class accept_handler : public net::socket_event_layer {
public:
  // -- member types -----------------------------------------------------------

  using socket_type = net::socket;

  using transport_type = typename Acceptor::transport_type;

  using connection_handle = typename transport_type::connection_handle;

  using factory_type = connection_factory<connection_handle>;

  using factory_ptr = detail::connection_factory_ptr<connection_handle>;

  // -- constructors, destructors, and assignment operators --------------------

  accept_handler(Acceptor acc, factory_ptr fptr, size_t max_connections,
                 std::vector<strong_actor_ptr> monitored_actors = {})
    : acc_(std::move(acc)),
      factory_(std::move(fptr)),
      max_connections_(max_connections),
      monitored_actors_(std::move(monitored_actors)) {
    CAF_ASSERT(max_connections_ > 0);
  }

  ~accept_handler() {
    on_conn_close_.dispose();
    if (valid(acc_))
      close(acc_);
    if (monitor_callback_)
      monitor_callback_.dispose();
  }

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<accept_handler>
  make(Acceptor acc, factory_ptr fptr, size_t max_connections,
       std::vector<strong_actor_ptr> monitored_actors = {}) {
    return std::make_unique<accept_handler>(std::move(acc), std::move(fptr),
                                            max_connections,
                                            std::move(monitored_actors));
  }

  // -- implementation of socket_event_layer -----------------------------------

  error start(net::socket_manager* owner) override {
    auto lg = log::net::trace("");
    owner_ = owner;
    if (auto err = factory_->start(owner)) {
      log::net::debug("factory_->start failed: {}", err);
      return err;
    }
    if (!monitored_actors_.empty()) {
      monitor_callback_ = make_action([this] { owner_->shutdown(); });
      auto ctx = async::execution_context_ptr{owner_->mpx_ptr()};
      for (auto& hdl : monitored_actors_) {
        CAF_ASSERT(hdl);
        hdl->get()->attach_functor([ctx, cb = monitor_callback_] {
          if (!cb.disposed())
            ctx->schedule(cb);
        });
      }
    }
    on_conn_close_ = make_action([this] { connection_closed(); });
    owner->register_reading();
    return none;
  }

  net::socket handle() const override {
    return acc_.fd();
  }

  void handle_read_event() override {
    auto lg = log::net::trace("");
    CAF_ASSERT(owner_ != nullptr);
    if (open_connections_.size() == max_connections_) {
      owner_->deregister_reading();
    } else if (auto conn = accept(acc_)) {
      auto child = factory_->make(owner_->mpx_ptr(), std::move(*conn));
      if (!child) {
        log::net::error("factory failed to create a new child");
        on_conn_close_.dispose();
        owner_->shutdown();
        return;
      }
      open_connections_.push_back(child->as_disposable());
      if (open_connections_.size() == max_connections_)
        owner_->deregister_reading();
      child->add_cleanup_listener(on_conn_close_);
      std::ignore = child->start();
    } else if (conn.error() == sec::unavailable_or_would_block) {
      // Encountered a "soft" error: simply try again later.
      log::net::debug("accept failed: {}", conn.error());
    } else {
      // Encountered a "hard" error: stop.
      abort(conn.error());
      owner_->deregister_reading();
    }
  }

  void handle_write_event() override {
    log::net::error("connection_acceptor received write event");
    owner_->deregister_writing();
  }

  void abort(const error& reason) override {
    log::net::error("connection_acceptor aborts due to an error: {}", reason);
    factory_->abort(reason);
    on_conn_close_.dispose();
    self_ref_ = nullptr;
    for (auto& hdl : open_connections_)
      hdl.dispose();
    open_connections_.clear();
  }

  void self_ref(disposable ref) {
    self_ref_ = std::move(ref);
  }

private:
  void connection_closed() {
    auto& conns = open_connections_;
    auto new_end = std::remove_if(conns.begin(), conns.end(),
                                  [](auto& ptr) { return ptr.disposed(); });
    if (new_end == conns.end())
      return;
    if (open_connections_.size() == max_connections_)
      owner_->register_reading();
    conns.erase(new_end, conns.end());
  }

  Acceptor acc_;

  factory_ptr factory_;

  size_t max_connections_;

  std::vector<disposable> open_connections_;

  net::socket_manager* owner_ = nullptr;

  action on_conn_close_;

  /// Type-erased handle to the @ref socket_manager. This reference is important
  /// to keep the acceptor alive while the manager is not registered for writing
  /// or reading.
  disposable self_ref_;

  /// An action for stopping this handler if an observed actor terminates.
  action monitor_callback_;

  /// List of actors that we add monitors to in `start`.
  std::vector<strong_actor_ptr> monitored_actors_;
};

} // namespace caf::detail
