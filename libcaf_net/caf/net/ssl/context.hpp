// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/dsl/arg.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/dtls.hpp"
#include "caf/net/ssl/format.hpp"
#include "caf/net/ssl/password.hpp"
#include "caf/net/ssl/tls.hpp"
#include "caf/net/ssl/verify.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/detail/net_export.hpp"
#include "caf/expected.hpp"
#include "caf/uri.hpp"

#include <cstring>
#include <string>
#include <type_traits>

namespace caf::net::ssl {

struct close_on_shutdown_t {};

constexpr close_on_shutdown_t close_on_shutdown = close_on_shutdown_t{};

/// SSL state, shared by multiple connections.
class CAF_NET_EXPORT context {
public:
  // -- member types -----------------------------------------------------------

  /// The opaque implementation type.
  struct impl;

  /// Stores additional data provided by the user.
  struct user_data;

  // -- constructors, destructors, and assignment operators --------------------

  context() = delete;

  explicit context(std::nullptr_t) : pimpl_(nullptr) {
    // nop
  }

  context(context&& other);

  context& operator=(context&& other);

  ~context();

  // -- factories --------------------------------------------------------------

  /// Starting point for chaining `expected<T>::and_then()` invocations, whereas
  /// the next function in the chain should create the SSL context depending on
  /// the value of `flag`.
  static expected<void> enable(bool flag);

  /// Returns a generic SSL context with TLS.
  static expected<context> make(tls min_version, tls max_version = tls::any);

  /// Returns a SSL context with TLS for a server role.
  static expected<context> make_server(tls min_version,
                                       tls max_version = tls::any);

  /// Returns a SSL context with TLS for a client role.
  static expected<context> make_client(tls min_version,
                                       tls max_version = tls::any);

  /// Returns a generic SSL context with DTLS.
  static expected<context> make(dtls min_version, dtls max_version = dtls::any);

  /// Returns a SSL context with DTLS for a server role.
  static expected<context> make_server(dtls min_version,
                                       dtls max_version = dtls::any);

  /// Returns a SSL context with TLS for a client role.
  static expected<context> make_client(dtls min_version,
                                       dtls max_version = dtls::any);

  // -- properties -------------------------------------------------------------

  explicit operator bool() const noexcept {
    return pimpl_ != nullptr;
  }

  bool operator!() const noexcept {
    return pimpl_ == nullptr;
  }

  /// Overrides the verification mode for this context.
  /// @note calls @c SSL_CTX_set_verify
  void verify_mode(verify_t flags);

  /// Overrides the callback to obtain the password for encrypted PEM files.
  /// @note calls @c SSL_CTX_set_default_passwd_cb
  template <typename PasswordCallback>
  void password_callback(PasswordCallback callback) {
    password_callback_impl(password::make_callback(std::move(callback)));
  }

  /// Overrides the callback to obtain the password for encrypted PEM files with
  /// a function that always returns @p password.
  /// @note calls @c SSL_CTX_set_default_passwd_cb
  void password(std::string password) {
    auto cb = [pw = std::move(password)](char* buf, int len,
                                         password::purpose) {
      strncpy(buf, pw.c_str(), static_cast<size_t>(len));
      buf[len - 1] = '\0';
      return static_cast<int>(pw.size());
    };
    password_callback(std::move(cb));
  }

  // -- native handles ---------------------------------------------------------

  /// Reinterprets `native_handle` as the native implementation type and takes
  /// ownership of the handle.
  static context from_native(void* native_handle);

  /// Retrieves the native handle from the context.
  void* native_handle() const noexcept;

  // -- error handling ---------------------------------------------------------

  /// Retrieves a human-readable error description for a preceding call to
  /// another member functions and removes that error from the thread-local
  /// error queue. Call repeatedly until @ref has_error returns `false` to
  /// retrieve all errors from the queue.
  static std::string next_error_string();

  /// Retrieves a human-readable error description for a preceding call to
  /// another member functions, appends the generated string to `buf` and
  /// removes that error from the thread-local error queue. Call repeatedly
  /// until @ref has_error returns `false` to retrieve all errors from the
  /// queue.
  static void append_next_error_string(std::string& buf);

  /// Convenience function for calling `next_error_string` repeatedly until
  /// @ref has_error returns `false`.
  static std::string last_error_string();

  /// Queries whether the thread-local error stack has at least one entry.
  static bool has_error() noexcept;

  /// Retrieves all errors from the thread-local error queue and assembles them
  /// into a single error string.
  /// @returns all error strings from the thread-local error queue or
  static error last_error();

  /// Returns @ref last_error or `default_error` if the former is
  /// default-constructed.
  static error last_error_or(error default_error);

  /// Returns @ref last_error or an error that represents an unexpected failure
  /// if the former is default-constructed.
  static error last_error_or_unexpected(std::string_view description);

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
  [[nodiscard]] bool enable_default_verify_paths();

  /// Configures the context to load CA certificate from a directory.
  /// @param path Null-terminated string with a path to a directory. Files in
  ///             the directory must use the CA subject name hash value as file
  ///             name with a suffix to disambiguate multiple certificates,
  ///             e.g., `9d66eef0.0` and `9d66eef0.1`.
  /// @returns `true` on success, `false` otherwise and `last_error` can be used
  ///          to retrieve a human-readable error representation.
  /// @note Calls @c SSL_CTX_load_verify_locations
  [[nodiscard]] bool add_verify_path(const char* path);

  /// @copydoc add_verify_path
  [[nodiscard]] bool add_verify_path(const std::string& path) {
    return add_verify_path(path.c_str());
  }

  /// Loads a CA certificate file.
  /// @param path Null-terminated string with a path to a single PEM file.
  /// @returns `true` on success, `false` otherwise and `last_error` can be used
  ///          to retrieve a human-readable error representation.
  /// @note Calls @c SSL_CTX_load_verify_locations
  [[nodiscard]] bool load_verify_file(const char* path);

  /// @copydoc load_verify_file
  [[nodiscard]] bool load_verify_file(const std::string& path) {
    return load_verify_file(path.c_str());
  }

  /// Loads the first certificate found in given file.
  /// @param path Null-terminated string with a path to a single file.
  /// @param file_format Denotes the format of the certificate file.
  [[nodiscard]] bool use_certificate_file(const char* path, format file_format);

  /// @copydoc use_certificate_file
  [[nodiscard]] bool use_certificate_file(const std::string& path,
                                          format file_format) {
    return use_certificate_file(path.c_str(), file_format);
  }

  /// Loads a certificate chain from a PEM-formatted file.
  /// @note calls @c SSL_CTX_use_certificate_chain_file
  [[nodiscard]] bool use_certificate_chain_file(const char* path);

  /// @copydoc use_certificate_chain_file
  [[nodiscard]] bool use_certificate_chain_file(const std::string& path) {
    return use_certificate_chain_file(path.c_str());
  }

  /// Loads the first private key found in given file.
  [[nodiscard]] bool use_private_key_file(const char* path, format file_format);

  /// @copydoc use_private_key_file
  [[nodiscard]] bool use_private_key_file(const std::string& path,
                                          format file_format) {
    return use_private_key_file(path.c_str(), file_format);
  }

  /// Sets the SNI (Server Name Indication) hostname for client connections
  /// created from this context.
  void sni_hostname(std::string hostname) noexcept;

  /// Returns the optional SNI hostname. Empty if SNI is not configured.
  const char* sni_hostname() const noexcept;

private:
  constexpr explicit context(impl* ptr) : pimpl_(ptr) {
    // nop
  }

  void password_callback_impl(password::callback_ptr callback);

  impl* pimpl_;
  user_data* data_ = nullptr;
};

} // namespace caf::net::ssl

namespace caf::detail {

// Convenience function for turning the Boolean results into a
// expected<context>.
inline expected<net::ssl::context>
ssl_ctx_chain(net::ssl::context& ctx, std::string_view descr, bool fn_res) {
  using net::ssl::context;
  if (fn_res)
    return expected<context>{std::move(ctx)};
  else
    return expected<context>{context::last_error_or_unexpected(descr)};
}

// Convenience function for calling a member function on the context with some
// arguments.
template <class... Ts, class... Args>
expected<net::ssl::context>
ssl_ctx_chain(net::ssl::context& ctx, std::string_view arg_check_error,
              std::string_view fn_error, bool (net::ssl::context::*fn)(Ts...),
              Args&... args) {
  using net::ssl::context;
  if ((!args && ...)) {
    auto err = make_error(sec::invalid_argument, std::string{arg_check_error});
    return expected<context>{std::move(err)};
  } else if ((ctx.*fn)(args.get()...)) {
    return expected<context>{std::move(ctx)};
  } else {
    return expected<context>{context::last_error_or_unexpected(fn_error)};
  }
}

// Convenience function for calling a member function on the context with some
// arguments. Unlike ssl_ctx_chain, this function does not result in an error if
// the arguments are invalid but simply returns the context as-is.
template <class... Ts, class... Args>
expected<net::ssl::context>
ssl_ctx_chain_if(net::ssl::context& ctx, std::string_view fn_error,
                 bool (net::ssl::context::*fn)(Ts...), Args&... args) {
  using net::ssl::context;
  if (!(args && ...) || (ctx.*fn)(args.get()...))
    return expected<context>{std::move(ctx)};
  else
    return expected<context>{context::last_error_or_unexpected(fn_error)};
}

} // namespace caf::detail

namespace caf::net::ssl {

// -- utility functions for turning expected<void> into an expected<context> ---

inline auto emplace_context(tls min_version, tls max_version = tls::any) {
  return [=] { return context::make(min_version, max_version); };
}

inline auto emplace_server(tls min_version, tls max_version = tls::any) {
  return [=] { return context::make_server(min_version, max_version); };
}

inline auto emplace_client(tls min_version, tls max_version = tls::any) {
  return [=] { return context::make_client(min_version, max_version); };
}

inline auto emplace_context(dtls min_version, dtls max_version = dtls::any) {
  return [=] { return context::make(min_version, max_version); };
}

inline auto emplace_server(dtls min_version, dtls max_version = dtls::any) {
  return [=] { return context::make_server(min_version, max_version); };
}

inline auto emplace_client(dtls min_version, dtls max_version = dtls::any) {
  return [=] { return context::make_client(min_version, max_version); };
}

// -- utility functions for chaining .and_then(...) on an expected<context> ----

/// Creates a new SSL connection on `fd`. The connection does not take ownership
/// of the socket, i.e., does not close the socket when the SSL session end or
/// on error.
/// @param fd the stream socket for adding encryption.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto new_connection(stream_socket fd) {
  // Note: this is a template to force the compiler to evaluate the body at a
  //       later time, because ssl::connection is incomplete here.
  return [fd](auto ctx) { return ctx.new_connection(fd); };
}

/// Creates a new SSL connection on `fd`. The connection takes ownership of
/// the socket, i.e., closes the socket when the SSL session ends.
/// @param fd the stream socket for adding encryption.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto new_connection(stream_socket fd, close_on_shutdown_t) {
  // Wrap into a guard to make sure the socket gets closed if this function
  // doesn't get called.
  return [guard = make_socket_guard(fd)](auto ctx) mutable {
    return ctx.new_connection(guard.release(), close_on_shutdown);
  };
}

/// Configure a context to use the default locations for loading CA
/// certificates.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto enable_default_verify_paths() {
  return [](context ctx) {
    return detail::ssl_ctx_chain(ctx, "enable_default_verify_paths failed",
                                 ctx.enable_default_verify_paths());
  };
}

/// Configures the context to load CA certificate from a directory.
/// @param path Null-terminated string with a path to a directory. Files in
///             the directory must use the CA subject name hash value as file
///             name with a suffix to disambiguate multiple certificates,
///             e.g., `9d66eef0.0` and `9d66eef0.1`.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto add_verify_path(dsl::arg::cstring path) {
  return [arg = std::move(path)](context ctx) {
    bool (context::*fn)(const char*) = &context::add_verify_path;
    return detail::ssl_ctx_chain(ctx, "add_verify_path: path cannot be null",
                                 "add_verify_path failed", fn, arg);
  };
}

/// Configures the context to load CA certificate from a directory if all
/// arguments are non-null. Otherwise, does nothing.
/// @param path Null-terminated string with a path to a directory. Files in
///             the directory must use the CA subject name hash value as file
///             name with a suffix to disambiguate multiple certificates,
///             e.g., `9d66eef0.0` and `9d66eef0.1`.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto add_verify_path_if(dsl::arg::cstring path) {
  return [arg = std::move(path)](context ctx) {
    bool (context::*fn)(const char*) = &context::add_verify_path;
    return detail::ssl_ctx_chain_if(ctx, "add_verify_path failed", fn, arg);
  };
}

/// Loads a CA certificate file.
/// @param path String with a path to a single PEM file.
/// @returns `true` on success, `false` otherwise and `last_error` can be used
///          to retrieve a human-readable error representation.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto load_verify_file(dsl::arg::cstring path) {
  return [arg = std::move(path)](context ctx) {
    bool (context::*fn)(const char*) = &context::load_verify_file;
    return detail::ssl_ctx_chain(ctx, "load_verify_file: path cannot be null",
                                 "load_verify_file failed", fn, arg);
  };
}

/// Loads a CA certificate file if all arguments are non-null. Otherwise, does
/// nothing.
/// @param path String with a path to a single PEM file.
/// @returns `true` on success, `false` otherwise and `last_error` can be used
///          to retrieve a human-readable error representation.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto load_verify_file_if(dsl::arg::cstring path) {
  return [arg = std::move(path)](context ctx) {
    bool (context::*fn)(const char*) = &context::load_verify_file;
    return detail::ssl_ctx_chain_if(ctx, "load_verify_file failed", fn, arg);
  };
}

/// @param password the stream socket for adding encryption.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto use_password(dsl::arg::cstring password) {
  return [password](auto ctx) -> expected<decltype(ctx)> {
    if (password.has_value())
      ctx.password(password.get());
    return ctx;
  };
}

/// @param password the stream socket for adding encryption.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto use_password_if(dsl::arg::cstring password) {
  return [password](auto ctx) -> expected<decltype(ctx)> {
    if (password.has_value())
      ctx.password(password.get());
    return ctx;
  };
}

/// Loads the first certificate found in given file.
/// @param path Null-terminated string with a path to a single file.
/// @param file_format Denotes the format of the certificate file.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto use_certificate_file(dsl::arg::cstring path,
                                 dsl::arg::val<format> file_format) {
  return [arg1 = std::move(path), arg2 = file_format](context ctx) mutable {
    bool (context::*fn)(const char*, format) = &context::use_certificate_file;
    return detail::ssl_ctx_chain(
      ctx, "use_certificate_file: path and file_format cannot be null",
      "use_certificate_file failed", fn, arg1, arg2);
  };
}

/// Loads the first certificate found in given file if all arguments are
/// non-null. Otherwise, does nothing.
/// @param path Null-terminated string with a path to a single file.
/// @param file_format Denotes the format of the certificate file.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto use_certificate_file_if(dsl::arg::cstring path,
                                    dsl::arg::val<format> file_format) {
  return [arg1 = std::move(path), arg2 = file_format](context ctx) mutable {
    bool (context::*fn)(const char*, format) = &context::use_certificate_file;
    return detail::ssl_ctx_chain_if(ctx, "use_certificate_file failed", fn,
                                    arg1, arg2);
  };
}

/// Loads a certificate chain from a PEM-formatted file.
/// @note calls @c SSL_CTX_use_certificate_chain_file
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto use_certificate_chain_file(dsl::arg::cstring path) {
  return [arg = std::move(path)](context ctx) mutable {
    bool (context::*fn)(const char*) = &context::use_certificate_chain_file;
    return detail::ssl_ctx_chain(
      ctx, "use_certificate_chain_file: path cannot be null",
      "use_certificate_chain_file failed", fn, arg);
  };
}

/// Loads a certificate chain from a PEM-formatted file if all arguments are
/// non-null. Otherwise, does nothing.
/// @note calls @c SSL_CTX_use_certificate_chain_file
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto use_certificate_chain_file_if(dsl::arg::cstring path) {
  return [arg = std::move(path)](context ctx) mutable {
    bool (context::*fn)(const char*) = &context::use_certificate_chain_file;
    return detail::ssl_ctx_chain_if(ctx, "use_certificate_chain_file failed",
                                    fn, arg);
  };
}

/// Loads the first private key found in given file.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto use_private_key_file(dsl::arg::cstring path,
                                 dsl::arg::val<format> file_format) {
  return [arg1 = std::move(path), arg2 = file_format](context ctx) mutable {
    bool (context::*fn)(const char*, format) = &context::use_private_key_file;
    return detail::ssl_ctx_chain(
      ctx, "use_private_key_file: path and file_format cannot be null",
      "use_private_key_file failed", fn, arg1, arg2);
  };
}

/// Loads the first private key found in given file if all arguments are
/// non-null. Otherwise, does nothing.
/// @returns a function object for chaining `expected<T>::and_then()`.
inline auto use_private_key_file_if(dsl::arg::cstring path,
                                    dsl::arg::val<format> file_format) {
  return [arg1 = std::move(path), arg2 = file_format](context ctx) mutable {
    bool (context::*fn)(const char*, format) = &context::use_private_key_file;
    return detail::ssl_ctx_chain_if(ctx, "use_private_key_file failed", fn,
                                    arg1, arg2);
  };
}

inline auto use_sni_hostname(std::string sni_hostname) noexcept {
  return [arg1 = std::move(sni_hostname)](context ctx) mutable {
    ctx.sni_hostname(std::move(arg1));
    return expected{std::move(ctx)};
  };
}

inline auto use_sni_hostname(caf::uri uri) noexcept {
  return [arg1 = std::move(uri)](context ctx) mutable -> expected<context> {
    auto& host = arg1.authority().host;
    if (std::holds_alternative<std::string>(host)) {
      ctx.sni_hostname(std::get<std::string>(host));
      return expected{std::move(ctx)};
    }
    return make_error(
      sec::runtime_error,
      "Failed to set SNI hostname: URI doesn't contain a valid hostname");
  };
}

} // namespace caf::net::ssl
