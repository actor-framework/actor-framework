// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/internal/accept_handler.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/actor_control_block.hpp"

namespace caf::internal {

namespace {

/// Accepts incoming clients with an acceptor and optionally monitors a set of
/// configurable actors.
class accept_handler_impl : public net::socket_event_layer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  accept_handler_impl(detail::connection_acceptor_ptr acceptor,
                      size_t max_connections,
                      std::vector<strong_actor_ptr> monitored_actors = {})
    : acceptor_(std::move(acceptor)),
      max_connections_(max_connections),
      monitored_actors_(std::move(monitored_actors)) {
    CAF_ASSERT(max_connections_ > 0);
  }

  ~accept_handler_impl() {
    on_conn_close_.dispose();
    if (monitor_callback_)
      monitor_callback_.dispose();
  }

  // -- implementation of socket_event_layer -----------------------------------

  error start(net::socket_manager* owner) override {
    auto lg = log::net::trace("");
    owner_ = owner;
    if (auto err = acceptor_->start(owner)) {
      log::net::debug("failed to start the acceptor: {}", err);
      return err;
    }
    self_ref_ = owner->as_disposable();
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
    return acceptor_->handle();
  }

  void handle_read_event() override {
    auto lg = log::net::trace("");
    CAF_ASSERT(owner_ != nullptr);
    if (open_connections_.size() == max_connections_) {
      owner_->deregister_reading();
      return;
    }
    if (auto conn = acceptor_->try_accept()) {
      auto& child = *conn;
      open_connections_.push_back(child->as_disposable());
      if (open_connections_.size() == max_connections_)
        owner_->deregister_reading();
      child->add_cleanup_listener(on_conn_close_);
      if (auto err = child->start()) {
        on_error(err);
      }
    } else if (conn.error() == sec::unavailable_or_would_block) {
      // Encountered a "soft" error: simply try again later.
    } else {
      // Encountered a "hard" error: stop.
      log::net::error("failed to accept a new connection: {}", conn.error());
      on_error(conn.error());
    }
  }

  void handle_write_event() override {
    log::net::error("connection_acceptor received write event");
    owner_->deregister_writing();
  }

  void abort(const error& reason) override {
    log::net::error("connection_acceptor aborts due to an error: {}", reason);
    acceptor_->abort(reason);
    on_conn_close_.dispose();
    self_ref_ = nullptr;
    for (auto& hdl : open_connections_)
      hdl.dispose();
    open_connections_.clear();
  }

private:
  void on_error(const error&) {
    on_conn_close_.dispose();
    self_ref_ = nullptr;
    for (auto& hdl : open_connections_)
      hdl.dispose();
    open_connections_.clear();
    owner_->shutdown();
  }

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

  detail::connection_acceptor_ptr acceptor_;

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

} // namespace

std::unique_ptr<net::socket_event_layer>
make_accept_handler(detail::connection_acceptor_ptr ptr, size_t max_connections,
                    std::vector<strong_actor_ptr> monitored_actors) {
  return std::make_unique<accept_handler_impl>(std::move(ptr), max_connections,
                                               std::move(monitored_actors));
}

} // namespace caf::internal
