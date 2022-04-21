// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_transport_error.hpp"

namespace caf::net {

template <class OnSuccess, class OnError>
struct default_handshake_worker_factory {
  OnSuccess make;
  OnError abort;
};

/// An connect worker calls an asynchronous `connect` callback until it
/// succeeds. On success, the worker calls a factory object to transfer
/// ownership of socket and communication policy to the create the socket
/// manager that takes care of the established connection.
template <bool IsServer, class Socket, class Policy, class Factory>
class handshake_worker : public socket_manager {
public:
  // -- member types -----------------------------------------------------------

  using super = socket_manager;

  using read_result = typename super::read_result;

  using write_result = typename super::write_result;

  handshake_worker(Socket handle, multiplexer* parent, Policy policy,
                   Factory factory)
    : super(handle, parent),
      policy_(std::move(policy)),
      factory_(std::move(factory)) {
    // nop
  }

  // -- interface functions ----------------------------------------------------

  error init(const settings& config) override {
    cfg_ = config;
    register_writing();
    return caf::none;
  }

  read_result handle_read_event() override {
    auto fd = socket_cast<Socket>(this->handle());
    if (auto res = advance_handshake(fd); res > 0) {
      return read_result::handover;
    } else if (res == 0) {
      factory_.abort(make_error(sec::connection_closed));
      return read_result::stop;
    } else {
      auto err = policy_.last_error(fd, res);
      switch (err) {
        case stream_transport_error::want_read:
        case stream_transport_error::temporary:
          return read_result::again;
        case stream_transport_error::want_write:
          return read_result::want_write;
        default:
          auto err = make_error(sec::cannot_connect_to_node,
                                policy_.fetch_error_str());
          factory_.abort(std::move(err));
          return read_result::stop;
      }
    }
  }

  read_result handle_buffered_data() override {
    return read_result::again;
  }

  read_result handle_continue_reading() override {
    return read_result::again;
  }

  write_result handle_write_event() override {
    auto fd = socket_cast<Socket>(this->handle());
    if (auto res = advance_handshake(fd); res > 0) {
      return write_result::handover;
    } else if (res == 0) {
      factory_.abort(make_error(sec::connection_closed));
      return write_result::stop;
    } else {
      switch (policy_.last_error(fd, res)) {
        case stream_transport_error::want_write:
        case stream_transport_error::temporary:
          return write_result::again;
        case stream_transport_error::want_read:
          return write_result::want_read;
        default:
          auto err = make_error(sec::cannot_connect_to_node,
                                policy_.fetch_error_str());
          factory_.abort(std::move(err));
          return write_result::stop;
      }
    }
  }

  write_result handle_continue_writing() override {
    return write_result::again;
  }

  void handle_error(sec code) override {
    factory_.abort(make_error(code));
  }

  socket_manager_ptr make_next_manager(socket hdl) override {
    auto ptr = factory_.make(socket_cast<Socket>(hdl), this->mpx_ptr(),
                             std::move(policy_));
    if (ptr) {
      if (auto err = ptr->init(cfg_)) {
        factory_.abort(err);
        return nullptr;
      } else {
        return ptr;
      }
    } else {
      factory_.abort(make_error(sec::runtime_error, "factory_.make failed"));
      return nullptr;
    }
  }

private:
  ptrdiff_t advance_handshake(Socket fd) {
    if constexpr (IsServer)
      return policy_.accept(fd);
    else
      return policy_.connect(fd);
  }

  settings cfg_;
  Policy policy_;
  Factory factory_;
};

} // namespace caf::net
