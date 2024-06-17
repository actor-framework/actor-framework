// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/ssl/connection.hpp"

#include "caf/net/ssl/context.hpp"

#include "caf/config.hpp"

CAF_PUSH_WARNINGS
#include <openssl/err.h>
#include <openssl/ssl.h>
CAF_POP_WARNINGS

namespace caf::net::ssl {

namespace {

auto native(connection::impl* ptr) {
  return reinterpret_cast<SSL*>(ptr);
}

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

connection::connection(connection&& other) {
  pimpl_ = other.pimpl_;
  other.pimpl_ = nullptr;
}

connection& connection::operator=(connection&& other) {
  SSL_free(native(pimpl_));
  pimpl_ = other.pimpl_;
  other.pimpl_ = nullptr;
  return *this;
}

connection::~connection() {
  SSL_free(native(pimpl_)); // Already checks for NULL.
}

// -- native handles -----------------------------------------------------------

connection connection::from_native(void* native_handle) {
  return connection{static_cast<impl*>(native_handle)};
}

void* connection::native_handle() const noexcept {
  return static_cast<void*>(pimpl_);
}

// -- error handling -----------------------------------------------------------

std::string connection::last_error_string(ptrdiff_t ret) const {
  auto code = last_error(ret);
  switch (code) {
    default:
      return to_string(code);
    case errc::fatal:
      return context::last_error_string();
    case errc::syscall_failed:
      return last_socket_error_as_string();
  }
}

errc connection::last_error(ptrdiff_t ret) const {
  auto code = SSL_get_error(native(pimpl_), static_cast<int>(ret));
  return detail::ssl_errc_from_native(code);
}

// -- connecting and teardown --------------------------------------------------

ptrdiff_t connection::connect() {
  ERR_clear_error();
  return SSL_connect(native(pimpl_));
}

ptrdiff_t connection::accept() {
  ERR_clear_error();
  return SSL_accept(native(pimpl_));
}

ptrdiff_t connection::close() {
  ERR_clear_error();
  return SSL_shutdown(native(pimpl_));
}

// -- reading and writing ------------------------------------------------------

ptrdiff_t connection::read(byte_span buf) {
  ERR_clear_error();
  return SSL_read(native(pimpl_), buf.data(), static_cast<int>(buf.size()));
}

ptrdiff_t connection::write(const_byte_span buf) {
  ERR_clear_error();
  return SSL_write(native(pimpl_), buf.data(), static_cast<int>(buf.size()));
}

// -- properties ---------------------------------------------------------------

size_t connection::buffered() const noexcept {
  return static_cast<size_t>(SSL_pending(native(pimpl_)));
}

stream_socket connection::fd() const noexcept {
  if (pimpl_ != nullptr) {
    if (auto id = SSL_get_fd(native(pimpl_)); id != -1) {
      return stream_socket{static_cast<socket_id>(id)};
    }
  }
  return stream_socket{invalid_socket_id};
}

} // namespace caf::net::ssl
