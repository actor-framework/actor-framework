// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/net/ssl/dtls.hpp"
#include "caf/net/ssl/format.hpp"
#include "caf/net/ssl/fwd.hpp"
#include "caf/net/ssl/tls.hpp"
#include "caf/net/stream_socket.hpp"

namespace caf::net::ssl {

struct close_on_shutdown_t {};

constexpr close_on_shutdown_t close_on_shutdown = close_on_shutdown_t{};

/// SSL state, shared by multiple connections.
class CAF_NET_EXPORT context {
public:
  // -- member types -----------------------------------------------------------

  /// The opaque implementation type.
  struct impl;

  // -- constructors, destructors, and assignment operators --------------------

  context() = delete;

  context(context&& other);

  context& operator=(context&& other);

  ~context();

  // -- factories --------------------------------------------------------------

  static expected<context> make(tls min_version, tls max_version = tls::any);

  static expected<context> make_server(tls min_version,
                                       tls max_version = tls::any);

  static expected<context> make_client(tls min_version,
                                       tls max_version = tls::any);

  static expected<context> make(dtls min_version, dtls max_version = dtls::any);

  static expected<context> make_server(dtls min_version,
                                       dtls max_version = dtls::any);

  static expected<context> make_client(dtls min_version,
                                       dtls max_version = dtls::any);

  // -- native handles ---------------------------------------------------------

  /// Reinterprets `native_handle` as the native implementation type and takes
  /// ownership of the handle.
  static context from_native(void* native_handle);

  /// Retrieves the native handle from the context.
  void* native_handle() const noexcept;

  // -- error handling ---------------------------------------------------------

  /// Retrieves a human-readable error description for a preceding call to
  /// another member functions and removes that error from the error queue. Call
  /// repeatedly until @ref has_last_error returns `false` to retrieve all
  /// errors from the queue.
  static std::string last_error_string();

  /// Queries whether the error stack has at least one entry.
  static bool has_last_error() noexcept;

  // -- connections ------------------------------------------------------------

  /// Creates a new SSL connection on `fd`. The connection does not take
  /// ownership of the socket, i.e., does not close the socket when the SSL
  /// session ends.
  expected<connection> new_connection(stream_socket fd);

  /// Creates a new SSL connection on `fd`. The connection takes ownership of
  /// the socket, i.e., closes the socket when the SSL session ends.
  expected<connection> new_connection(stream_socket fd, close_on_shutdown_t);

  // -- certificates and keys --------------------------------------------------

  /// Configure the context to use the default locations for loading CA
  /// certificates.
  /// @returns `true` on success, `false` otherwise and `last_error` can be used
  ///          to retrieve a human-readable error representation.
  [[nodiscard]] bool set_default_verify_paths();

  /// Configures the context to load CA certificate from a directory.
  /// @param path Null-terminated string with a path to a directory. Files in
  ///             the directory must use the CA subject name hash value as file
  ///             name with a suffix to disambiguate multiple certificates,
  ///             e.g., `9d66eef0.0` and `9d66eef0.1`.
  /// @returns `true` on success, `false` otherwise and `last_error` can be used
  ///          to retrieve a human-readable error representation.
  [[nodiscard]] bool load_verify_dir(const char* path);

  /// Loads a CA certificate file.
  /// @param path Null-terminated string with a path to a single PEM file.
  /// @returns `true` on success, `false` otherwise and `last_error` can be used
  ///          to retrieve a human-readable error representation.
  [[nodiscard]] bool load_verify_file(const char* path);

  /// Loads the first certificate found in given file.
  /// @param path Null-terminated string with a path to a single PEM file.
  [[nodiscard]] bool use_certificate_from_file(const char* path,
                                               format file_format);

  /// Loads the first private key found in given file.
  [[nodiscard]] bool use_private_key_from_file(const char* path,
                                               format file_format);

private:
  constexpr explicit context(impl* ptr) : pimpl_(ptr) {
    // nop
  }

  impl* pimpl_;
};

} // namespace caf::net::ssl
