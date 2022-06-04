// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/handshake_worker.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/sec.hpp"
#include "caf/settings.hpp"
#include "caf/span.hpp"

CAF_PUSH_WARNINGS
#include <openssl/err.h>
#include <openssl/ssl.h>
CAF_POP_WARNINGS

#include <memory>
#include <string>
#include <string_view>

namespace caf::net::ssl {

/// State shared by multiple connections.
class context {
public:
  struct impl;

  context() = delete;

  context(context&& other);

  context& operator=(context&& other);

private:
};

/// State per connection.
struct connection;

/// Deletes an SSL object.
struct deleter {
  void operator()(context* ptr) const noexcept;

  void operator()(connection* ptr) const noexcept;
};

/// An owning smart pointer for a @ref context.
using ctx_ptr = std::unique_ptr<context, deleter>;

/// An owning smart pointer for a @ref connection.
using conn_ptr = std::unique_ptr<connection, deleter>;

/// Convenience function for creating an OpenSSL context for given method.
inline ctx_ptr make_ctx(const SSL_METHOD* method) {
  if (auto ptr = SSL_CTX_new(method))
    return ctx_ptr{ptr};
  else
    CAF_RAISE_ERROR("SSL_CTX_new failed");
}

/// Fetches a string representation for the last OpenSSL errors.
std::string fetch_error_str() {
  auto cb = [](const char* cstr, size_t len, void* vptr) -> int {
    auto& str = *reinterpret_cast<std::string*>(vptr);
    if (str.empty()) {
      str.assign(cstr, len);
    } else {
      str += "; ";
      auto view = std::string_view{cstr, len};
      str.insert(str.end(), view.begin(), view.end());
    }
    return 1;
  };
  std::string result;
  ERR_print_errors_cb(cb, &result);
  return result;
}

///  Loads the certificate into the SSL context.
inline error certificate_pem_file(const ctx_ptr& ctx, const std::string& path) {
  auto cstr = path.c_str();
  if (SSL_CTX_use_certificate_file(ctx.get(), cstr, SSL_FILETYPE_PEM) > 0) {
    return none;
  } else {
    return make_error(sec::invalid_argument, fetch_error_str());
  }
}

///  Loads the private key into the SSL context.
inline error private_key_pem_file(const ctx_ptr& ctx, const std::string& path) {
  auto cstr = path.c_str();
  if (SSL_CTX_use_PrivateKey_file(ctx.get(), cstr, SSL_FILETYPE_PEM) > 0) {
    return none;
  } else {
    return make_error(sec::invalid_argument, fetch_error_str());
  }
}

/// Convenience function for creating a new SSL structure from given context.
inline conn_ptr make_conn(const ctx_ptr& ctx) {
  if (auto ptr = SSL_new(ctx.get()))
    return conn_ptr{ptr};
  else
    CAF_RAISE_ERROR("SSL_new failed");
}

/// Convenience function for creating a new SSL structure from given context and
/// binding the given socket to it.
inline conn_ptr make_conn(const ctx_ptr& ctx, stream_socket fd) {
  auto ptr = make_conn(ctx);
  if (SSL_set_fd(ptr.get(), fd.id))
    return ptr;
  else
    CAF_RAISE_ERROR("SSL_set_fd failed");
}

/// Manages an OpenSSL connection.
class policy : public stream_transport::policy {
public:
  // -- constructors, destructors, and assignment operators --------------------

  policy() = delete;

  policy(const policy&) = delete;

  policy& operator=(const policy&) = delete;

  policy(policy&&) = default;

  policy& operator=(policy&&) = default;

  explicit policy(conn_ptr conn) : conn_(std::move(conn)) {
    // nop
  }

  // -- factories --------------------------------------------------------------

  /// Creates a policy from an SSL context and socket.
  static policy make(const ctx_ptr& ctx, stream_socket fd) {
    return policy{make_conn(ctx, fd)};
  }

  /// Creates a policy from an SSL method and socket.
  static policy make(const SSL_METHOD* method, stream_socket fd) {
    auto ctx = make_ctx(method);
    return policy{make_conn(ctx, fd)};
  }

  // -- properties -------------------------------------------------------------

  SSL* conn() {
    return conn_.get();
  }

  // -- OpenSSL settings -------------------------------------------------------

  ///  Loads the certificate into the SSL connection object.
  error certificate_pem_file(const std::string& path) {
    auto cstr = path.c_str();
    if (SSL_use_certificate_file(conn(), cstr, SSL_FILETYPE_PEM) > 0) {
      return none;
    } else {
      return make_error(sec::invalid_argument, fetch_error_str());
    }
  }

  ///  Loads the private key into the SSL connection object.
  error private_key_pem_file(const std::string& path) {
    auto cstr = path.c_str();
    if (SSL_use_PrivateKey_file(conn(), cstr, SSL_FILETYPE_PEM) > 0) {
      return none;
    } else {
      return make_error(sec::invalid_argument, fetch_error_str());
    }
  }

  /// Graceful shutdown.
  void notify_close() {
    SSL_shutdown(conn_.get());
  }

  // -- implementation of stream_transport::policy -----------------------------

  /// Fetches a string representation for the last error.
  std::string fetch_error_str() {
    return openssl::fetch_error_str();
  }

  ptrdiff_t read(stream_socket, byte_span) override;

  ptrdiff_t write(stream_socket, const_byte_span) override;

  stream_transport_error last_error(stream_socket, ptrdiff_t) override;

  ptrdiff_t connect(stream_socket) override;

  ptrdiff_t accept(stream_socket) override;

  size_t buffered() const noexcept override;

private:
  /// Our SSL connection data.
  openssl::conn_ptr conn_;
};

/// Implements a stream_transport that manages a stream socket with encrypted
/// communication over OpenSSL.
class CAF_NET_EXPORT transport : public stream_transport {
public:
  // -- member types -----------------------------------------------------------

  using super = stream_transport;

  using openssl_transport_ptr = std::unique_ptr<openssl_transport>;

  using worker_ptr = std::unique_ptr<socket_event_layer>;

  // -- factories --------------------------------------------------------------

  /// Creates a new instance of the SSL transport for a socket that has already
  /// performed the SSL handshake.
  static openssl_transport_ptr make(stream_socket fd, openssl::policy policy,
                                    upper_layer_ptr up);

  /// Returns a worker that performs the OpenSSL server handshake on the socket.
  /// On success, the worker performs a handover to an `openssl_transport` that
  /// runs `up`.
  static worker_ptr make_server(stream_socket fd, openssl::policy policy,
                                upper_layer_ptr up);

  /// Returns a worker that performs the OpenSSL client handshake on the socket.
  /// On success, the worker performs a handover to an `openssl_transport` that
  /// runs `up`.
  static worker_ptr make_client(stream_socket fd, openssl::policy policy,
                                upper_layer_ptr up);

private:
  // -- constructors, destructors, and assignment operators --------------------

  openssl_transport(stream_socket fd, openssl::policy pol, upper_layer_ptr up);

  openssl::policy ssl_policy_;
};

} // namespace caf::net::ssl
