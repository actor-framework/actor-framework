// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/socket_event_layer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/settings.hpp"

namespace caf::net {

/// A connection_acceptor accepts connections from an accept socket and creates
/// socket managers to handle them via its factory.
template <class Socket, class Factory>
class connection_acceptor : public socket_event_layer {
public:
  // -- member types -----------------------------------------------------------

  using socket_type = Socket;

  using factory_type = Factory;

  using read_result = typename socket_event_layer::read_result;

  using write_result = typename socket_event_layer::write_result;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit connection_acceptor(Socket fd, size_t limit, Ts&&... xs)
    : fd_(fd), limit_(limit), factory_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- factories --------------------------------------------------------------

  template <class... Ts>
  static std::unique_ptr<connection_acceptor>
  make(Socket fd, size_t limit, Ts&&... xs) {
    return std::make_unique<connection_acceptor>(fd, limit,
                                                 std::forward<Ts>(xs)...);
  }

  // -- implementation of socket_event_layer -----------------------------------

  error init(socket_manager* owner, const settings& cfg) override {
    CAF_LOG_TRACE("");
    owner_ = owner;
    cfg_ = cfg;
    if (auto err = factory_.init(owner, cfg))
      return err;
    owner->register_reading();
    return none;
  }

  read_result handle_read_event() override {
    CAF_LOG_TRACE("");
    if (auto x = accept(fd_)) {
      socket_manager_ptr child = factory_.make(owner_->mpx_ptr(), *x);
      if (!child) {
        CAF_LOG_ERROR("factory failed to create a new child");
        return read_result::abort;
      }
      if (auto err = child->init(cfg_)) {
        CAF_LOG_ERROR("failed to initialize new child:" << err);
        return read_result::abort;
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

  read_result handle_buffered_data() override {
    return read_result::again;
  }

  read_result handle_continue_reading() override {
    return read_result::again;
  }

  write_result handle_write_event() override {
    CAF_LOG_ERROR("connection_acceptor received write event");
    return write_result::stop;
  }

  void abort(const error& reason) override {
    CAF_LOG_ERROR("connection_acceptor aborts due to an error: " << reason);
    factory_.abort(reason);
  }

private:
  Socket fd_;

  size_t limit_;

  factory_type factory_;

  socket_manager* owner_ = nullptr;

  size_t accepted_ = 0;

  settings cfg_;
};

} // namespace caf::net
