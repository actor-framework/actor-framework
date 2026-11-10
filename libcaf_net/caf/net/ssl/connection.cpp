// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/ssl/connection.hpp"

#include "caf/net/ssl/context.hpp"

#include "caf/config.hpp"
#include "caf/error.hpp"
#include "caf/ip_address.hpp"
#include "caf/log/net.hpp"

CAF_PUSH_WARNINGS
#include <openssl/err.h>
#include <openssl/ssl.h>
CAF_POP_WARNINGS

#include <string_view>

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

// -- SSL hostname validation and SNI support ----------------------------------

bool connection::sni_hostname(const char* hostname) noexcept {
  if (!pimpl_ || !hostname)
    return false;
  log::net::debug("Setting SNI hostname to {}", hostname);
  return SSL_set_tlsext_host_name(native(pimpl_), hostname);
}

const char* connection::sni_hostname() noexcept {
  if (!pimpl_)
    return nullptr;
  return SSL_get_servername(native(pimpl_), TLSEXT_NAMETYPE_host_name);
}

bool connection::hostname(const char* hoststr) noexcept {
  if (!pimpl_)
    return false;
  ip_address addr;
  auto is_ip = hoststr && parse(hoststr, addr).empty();
  if (is_ip) {
    // Older OpenSSL versions accept ambiguous IPv4 spellings that the socket
    // resolver may interpret differently. Check both IPv4 literals and the
    // dotted IPv4 suffix of an IPv6 literal without normalizing the IPv6 part.
    auto v4_part = std::string_view{hoststr};
    if (auto pos = v4_part.rfind(':'); pos != std::string_view::npos)
      v4_part.remove_prefix(pos + 1);
    ipv4_address v4;
    if (parse(v4_part, v4).empty() && to_string(v4) != v4_part)
      return false;
  }
#if OPENSSL_VERSION_NUMBER >= 0x40000000L
  // OpenSSL 4 maintains separate DNS and IP lists. Clear the list that we do
  // not set so that a new identity replaces the previous one. Passing a
  // `nullptr` clears both lists.
  if (is_ip) {
    return SSL_set1_dnsname(native(pimpl_), nullptr)
           && SSL_set1_ipaddr(native(pimpl_), hoststr);
  }
  return SSL_set1_ipaddr(native(pimpl_), nullptr)
         && SSL_set1_dnsname(native(pimpl_), hoststr);
#else
  // Use the X509 parameters for IP addresses on older OpenSSL versions.
  // Passing a `nullptr` clears both lists.
  auto* params = SSL_get0_param(native(pimpl_));
  if (is_ip) {
    return X509_VERIFY_PARAM_set1_host(params, nullptr, 0)
           && X509_VERIFY_PARAM_set1_ip_asc(params, hoststr);
  }
  return X509_VERIFY_PARAM_set1_ip(params, nullptr, 0)
         && SSL_set1_host(native(pimpl_), hoststr);
#endif
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
