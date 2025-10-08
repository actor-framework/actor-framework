// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/ssl/transport.hpp"

#include "caf/net/octet_stream/errc.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/ssl/connection.hpp"

#include "caf/error.hpp"

CAF_PUSH_WARNINGS
#include <openssl/err.h>
#include <openssl/ssl.h>
CAF_POP_WARNINGS

namespace caf::net::ssl {

namespace {

class policy_impl : public octet_stream::policy {
public:
  explicit policy_impl(connection conn) : conn(std::move(conn)) {
    // nop
  }

  stream_socket handle() const override {
    return conn.fd();
  }

  ptrdiff_t read(byte_span buf) override {
    return conn.read(buf);
  }

  ptrdiff_t write(const_byte_span buf) override {
    return conn.write(buf);
  }

  octet_stream::errc last_error(ptrdiff_t ret) override {
    switch (conn.last_error(ret)) {
      case errc::none:
      case errc::want_accept:
      case errc::want_connect:
        // For all of these, OpenSSL docs say to do the operation again later.
        return octet_stream::errc::temporary;
      case errc::syscall_failed:
        // Need to consult errno, which we just leave to the default policy.
        return last_socket_error_is_temporary() ? octet_stream::errc::temporary
                                                : octet_stream::errc::permanent;
      case errc::want_read:
        return octet_stream::errc::want_read;
      case errc::want_write:
        return octet_stream::errc::want_write;
      default:
        // Errors like SSL_ERROR_WANT_X509_LOOKUP are technically temporary, but
        // we do not configure any callbacks. So seeing this is a red flag.
        return octet_stream::errc::permanent;
    }
  }

  ptrdiff_t connect() override {
    return conn.connect();
  }

  ptrdiff_t accept() override {
    return conn.accept();
  }

  size_t buffered() const noexcept override {
    return conn.buffered();
  }

  connection conn;
};

/// Calls `connect` or `accept` until it succeeds or fails. On success, the
/// worker creates a new SSL transport and performs a handover.
class handshake_worker : public socket_event_layer {
public:
  // -- member types -----------------------------------------------------------

  using super = socket_event_layer;

  using upper_layer_ptr = transport::upper_layer_ptr;

  handshake_worker(connection conn, bool is_server, upper_layer_ptr up)
    : is_server_(is_server), policy_(std::move(conn)), up_(std::move(up)) {
    // nop
  }

  // -- interface functions ----------------------------------------------------

  error start(socket_manager* owner) override {
    owner_ = owner;
    owner->register_writing();
    return caf::none;
  }

  socket handle() const override {
    return policy_.conn.fd();
  }

  void handle_read_event() override {
    if (auto res = advance_handshake(); res > 0) {
      owner_->deregister();
      owner_->schedule_handover();
    } else if (res == 0) {
      up_->abort(make_error(sec::connection_closed));
      owner_->deregister();
    } else {
      auto err = policy_.last_error(res);
      switch (err) {
        case octet_stream::errc::want_read:
        case octet_stream::errc::temporary:
          break;
        case octet_stream::errc::want_write:
          owner_->deregister_reading();
          owner_->register_writing();
          break;
        default: {
          auto err = make_error(sec::cannot_connect_to_node,
                                policy_.conn.last_error_string(res));
          up_->abort(std::move(err));
          owner_->deregister();
        }
      }
    }
  }

  void handle_write_event() override {
    if (auto res = advance_handshake(); res > 0) {
      owner_->deregister();
      owner_->schedule_handover();
      return;
    } else if (res == 0) {
      up_->abort(make_error(sec::connection_closed));
      owner_->deregister();
    } else {
      switch (policy_.last_error(res)) {
        case octet_stream::errc::want_write:
        case octet_stream::errc::temporary:
          break;
        case octet_stream::errc::want_read:
          owner_->deregister_writing();
          owner_->register_reading();
          break;
        default: {
          auto err = make_error(sec::cannot_connect_to_node,
                                policy_.conn.last_error_string(res));
          up_->abort(std::move(err));
          owner_->deregister();
        }
      }
    }
  }

  void handle_custom_event(uint8_t opcode, uint64_t payload) override {
    up_->handle_custom_event(opcode, payload);
  }

  bool do_handover(std::unique_ptr<socket_event_layer>& next) override {
    next = transport::make(std::move(policy_.conn), std::move(up_));
    if (auto err = next->start(owner_))
      return false;
    else
      return true;
  }

  void abort(const error& reason) override {
    up_->abort(reason);
  }

private:
  ptrdiff_t advance_handshake() {
    if (is_server_)
      return policy_.conn.accept();
    else
      return policy_.conn.connect();
  }

  bool is_server_ = false;
  socket_manager* owner_ = nullptr;
  policy_impl policy_;
  upper_layer_ptr up_;
};

} // namespace

// -- factories ----------------------------------------------------------------

std::unique_ptr<octet_stream::transport> transport::make(connection conn,
                                                         upper_layer_ptr up) {
  return octet_stream::transport::make(
    std::make_unique<policy_impl>(std::move(conn)), std::move(up));
}

transport::worker_ptr transport::make_server(connection conn,
                                             upper_layer_ptr up) {
  return std::make_unique<handshake_worker>(std::move(conn), true,
                                            std::move(up));
}

transport::worker_ptr transport::make_client(connection conn,
                                             upper_layer_ptr up) {
  return std::make_unique<handshake_worker>(std::move(conn), false,
                                            std::move(up));
}

} // namespace caf::net::ssl
