// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/ssl/context.hpp"

#include "caf/config.hpp"
#include "caf/expected.hpp"
#include "caf/net/ssl/connection.hpp"

CAF_PUSH_WARNINGS
#include <openssl/err.h>
#include <openssl/ssl.h>
CAF_POP_WARNINGS

namespace caf::net::ssl {

namespace {

auto native(context::impl* ptr) {
  return reinterpret_cast<SSL_CTX*>(ptr);
}

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

context::context(context&& other) {
  pimpl_ = other.pimpl_;
  other.pimpl_ = nullptr;
}

context& context::operator=(context&& other) {
  SSL_CTX_free(native(pimpl_));
  pimpl_ = other.pimpl_;
  other.pimpl_ = nullptr;
  return *this;
}

context::~context() {
  SSL_CTX_free(native(pimpl_)); // Already checks for NULL.
}

// -- factories ----------------------------------------------------------------

namespace {

std::pair<SSL_CTX*, const char*> make_ctx(const SSL_METHOD* method, int vmin,
                                          int vmax) {
  auto ctx = SSL_CTX_new(method);
  if (!ctx)
    return {nullptr, "SSL_CTX_new returned null"};
  if (vmin != 0 && SSL_CTX_set_min_proto_version(ctx, vmin) != 1)
    return {ctx, "SSL_CTX_set_min_proto_version returned an error"};
  if (vmax != 0 && SSL_CTX_set_max_proto_version(ctx, vmax) != 1)
    return {ctx, "SSL_CTX_set_max_proto_version returned an error"};
  return {ctx, nullptr};
}

} // namespace

expected<context> context::make(tls min_version, tls max_version) {
  auto [raw, errstr] = make_ctx(TLS_method(), native(min_version),
                                native(max_version));
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

expected<context> context::make_server(tls min_version, tls max_version) {
  auto [raw, errstr] = make_ctx(TLS_server_method(), native(min_version),
                                native(max_version));
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

expected<context> context::make_client(tls min_version, tls max_version) {
  auto [raw, errstr] = make_ctx(TLS_client_method(), native(min_version),
                                native(max_version));
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

expected<context> context::make(dtls min_version, dtls max_version) {
  auto [raw, errstr] = make_ctx(TLS_method(), native(min_version),
                                native(max_version));
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

expected<context> context::make_server(dtls min_version, dtls max_version) {
  auto [raw, errstr] = make_ctx(DTLS_server_method(), native(min_version),
                                native(max_version));
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

expected<context> context::make_client(dtls min_version, dtls max_version) {
  auto [raw, errstr] = make_ctx(DTLS_client_method(), native(min_version),
                                native(max_version));
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

// -- native handles -----------------------------------------------------------

context context::from_native(void* native_handle) {
  return context{static_cast<impl*>(native_handle)};
}

void* context::native_handle() const noexcept {
  return static_cast<void*>(pimpl_);
}

// -- error handling -----------------------------------------------------------

std::string context::last_error_string() {
  auto save_cstr = [](const char* cstr) { return cstr ? cstr : "NULL"; };
  if (auto code = ERR_get_error(); code != 0) {
    std::string result;
    result.reserve(256);
    result = "error:";
    result += std::to_string(code);
    result += ':';
    result += save_cstr(ERR_lib_error_string(code));
    result += "::";
    result += save_cstr(ERR_reason_error_string(code));
    return result;
  } else {
    return "no-error";
  }
}

bool context::has_last_error() noexcept {
  return ERR_peek_error() != 0;
}

// -- connections --------------------------------------------------------------

expected<connection> context::new_connection(stream_socket fd) {
  if (auto ptr = SSL_new(native(pimpl_))) {
    auto conn = connection::from_native(ptr);
    if (SSL_set_fd(ptr, fd.id) == 1)
      return {std::move(conn)};
    else
      return {make_error(sec::logic_error, "SSL_set_fd failed")};

  } else {
    return {make_error(sec::logic_error, "SSL_new returned null")};
  }
}

// -- certificates and keys ----------------------------------------------------

bool context::set_default_verify_paths() {
  ERR_clear_error();
  return SSL_CTX_set_default_verify_paths(native(pimpl_)) == 1;
}

bool context::load_verify_dir(const char* path) {
  ERR_clear_error();
  return SSL_CTX_load_verify_locations(native(pimpl_), nullptr, path) == 1;
}

bool context::load_verify_file(const char* path) {
  ERR_clear_error();
  return SSL_CTX_load_verify_locations(native(pimpl_), path, nullptr) == 1;
}

bool context::use_certificate_from_file(const char* path, format file_format) {
  ERR_clear_error();
  return SSL_CTX_use_certificate_file(native(pimpl_), path, native(file_format))
         == 1;
}

bool context::use_private_key_from_file(const char* path, format file_format) {
  ERR_clear_error();
  return SSL_CTX_use_PrivateKey_file(native(pimpl_), path, native(file_format))
         == 1;
}

} // namespace caf::net::ssl
