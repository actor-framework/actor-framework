// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/ssl/context.hpp"

#include "caf/net/ssl/connection.hpp"

#include "caf/config.hpp"
#include "caf/expected.hpp"

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

// -- member types -------------------------------------------------------------

struct context::user_data {
  password::callback_ptr pw_callback;
};

// -- constructors, destructors, and assignment operators ----------------------

context::context(context&& other) {
  pimpl_ = other.pimpl_;
  other.pimpl_ = nullptr;
  data_ = other.data_;
  other.data_ = nullptr;
}

context& context::operator=(context&& other) {
  SSL_CTX_free(native(pimpl_));
  pimpl_ = other.pimpl_;
  other.pimpl_ = nullptr;
  delete data_;
  data_ = other.data_;
  other.data_ = nullptr;
  return *this;
}

context::~context() {
  SSL_CTX_free(native(pimpl_)); // Already checks for NULL.
  delete data_;
}

// -- factories ----------------------------------------------------------------

namespace {

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#  define CAF_TLS_METHOD(infix) SSLv23##infix##method()
#else
#  define CAF_TLS_METHOD(infix) TLS##infix##method()
#endif

template <class Enum>
std::pair<SSL_CTX*, const char*>
make_ctx(const SSL_METHOD* method, Enum min_val, Enum max_val) {
  if (min_val > max_val && max_val != Enum::any)
    return {nullptr, "invalid version range"};
  auto ctx = SSL_CTX_new(method);
  if (!ctx)
    return {nullptr, "SSL_CTX_new returned null"};
    // Avoid fallback to SSLv3.
    // Select the protocol versions in the configured range.
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  if constexpr (std::is_same_v<Enum, tls>) {
    auto opts = SSL_OP_NO_SSLv3;
    if (!has(tls::v1_0, min_val, max_val))
      opts |= SSL_OP_NO_TLSv1;
    if (!has(tls::v1_1, min_val, max_val))
      opts |= SSL_OP_NO_TLSv1_1;
    if (!has(tls::v1_2, min_val, max_val))
      opts |= SSL_OP_NO_TLSv1_2;
    SSL_CTX_set_options(ctx, opts);
  } else {
    static_assert(std::is_same_v<Enum, dtls>);
    // Nothing to do for DTLS in this OpenSSL version.
    CAF_IGNORE_UNUSED(min_val);
    CAF_IGNORE_UNUSED(max_val);
  }
#else
  if constexpr (std::is_same_v<Enum, tls>) {
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
  }
  auto vmin = native(min_val);
  auto vmax = native(max_val);
  if (vmin != 0 && SSL_CTX_set_min_proto_version(ctx, vmin) != 1)
    return {ctx, "SSL_CTX_set_min_proto_version returned an error"};
  if (vmax != 0 && SSL_CTX_set_max_proto_version(ctx, vmax) != 1)
    return {ctx, "SSL_CTX_set_max_proto_version returned an error"};
#endif
  return {ctx, nullptr};
}

} // namespace

expected<void> context::enable(bool flag) {
  // By returning a default-constructed error, we suppress any subsequent
  // function calls in an `and_then` chain. The caf-net DSL then treats a
  // default-constructed error as "no SSL".
  if (flag)
    return expected<void>{};
  else
    return expected<void>{caf::error{}};
}

expected<context> context::make(tls vmin, tls vmax) {
  auto [raw, errstr] = make_ctx(CAF_TLS_METHOD(_), vmin, vmax);
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

expected<context> context::make_server(tls vmin, tls vmax) {
  auto [raw, errstr] = make_ctx(CAF_TLS_METHOD(_server_), vmin, vmax);
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

expected<context> context::make_client(tls vmin, tls vmax) {
  auto [raw, errstr] = make_ctx(CAF_TLS_METHOD(_client_), vmin, vmax);
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

expected<context> context::make(dtls vmin, dtls vmax) {
  auto [raw, errstr] = make_ctx(DTLS_method(), vmin, vmax);
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

expected<context> context::make_server(dtls vmin, dtls vmax) {
  auto [raw, errstr] = make_ctx(DTLS_server_method(), vmin, vmax);
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

expected<context> context::make_client(dtls vmin, dtls vmax) {
  auto [raw, errstr] = make_ctx(DTLS_client_method(), vmin, vmax);
  context ctx{reinterpret_cast<impl*>(raw)};
  if (errstr == nullptr)
    return {std::move(ctx)};
  else
    return {make_error(sec::logic_error, errstr)};
}

// -- properties ---------------------------------------------------------------

void context::verify_mode(verify_t flags) {
  auto ptr = native(pimpl_);
  SSL_CTX_set_verify(ptr, to_integer(flags), SSL_CTX_get_verify_callback(ptr));
}

namespace {

int c_password_callback(char* buf, int size, int rwflag, void* ptr) {
  if (ptr == nullptr)
    return -1;
  auto flag = static_cast<password::purpose>(rwflag);
  auto fn = static_cast<password::callback*>(ptr);
  return (*fn)(buf, size, flag);
}

} // namespace

void context::password_callback_impl(password::callback_ptr callback) {
  if (data_ == nullptr)
    data_ = new user_data;
  auto ptr = native(pimpl_);
  data_->pw_callback = std::move(callback);
  SSL_CTX_set_default_passwd_cb(ptr, c_password_callback);
  SSL_CTX_set_default_passwd_cb_userdata(ptr, data_->pw_callback.get());
}

// -- native handles -----------------------------------------------------------

context context::from_native(void* native_handle) {
  return context{static_cast<impl*>(native_handle)};
}

void* context::native_handle() const noexcept {
  return static_cast<void*>(pimpl_);
}

// -- error handling -----------------------------------------------------------

std::string context::next_error_string() {
  std::string result;
  append_next_error_string(result);
  return result;
}

void context::append_next_error_string(std::string& buf) {
  auto save_cstr = [](const char* cstr) { return cstr ? cstr : "NULL"; };
  if (auto code = ERR_get_error(); code != 0) {
    buf = "error:";
    buf += std::to_string(code);
    buf += ':';
    buf += save_cstr(ERR_lib_error_string(code));
    buf += "::";
    buf += save_cstr(ERR_reason_error_string(code));
  } else {
    buf += "no-error";
  }
}

std::string context::last_error_string() {
  if (!has_error())
    return {};
  auto result = next_error_string();
  while (has_error()) {
    result += '\n';
    append_next_error_string(result);
  }
  return result;
}

bool context::has_error() noexcept {
  return ERR_peek_error() != 0;
}

error context::last_error() {
  if (ERR_peek_error() == 0)
    return error{};
  auto description = next_error_string();
  while (has_error()) {
    description += '\n';
    append_next_error_string(description);
  }
  // TODO: Mapping the codes to an error enum would be much nicer than using
  //       the generic 'runtime_error'.
  return make_error(sec::runtime_error, std::move(description));
}

error context::last_error_or(error default_error) {
  if (ERR_peek_error() == 0)
    return default_error;
  else
    return last_error();
}

error context::last_error_or_unexpected(std::string_view description) {
  if (ERR_peek_error() == 0)
    return make_error(sec::runtime_error, std::string{description});
  else
    return last_error();
}

// -- connections --------------------------------------------------------------

expected<connection> context::new_connection(stream_socket fd) {
  if (auto ptr = SSL_new(native(pimpl_))) {
    auto conn = connection::from_native(ptr);
    if (auto bio_ptr = BIO_new_socket(fd.id, BIO_NOCLOSE)) {
      SSL_set_bio(ptr, bio_ptr, bio_ptr);

      return {std::move(conn)};
    } else {
      return {make_error(sec::logic_error, "BIO_new_socket failed")};
    }
  } else {
    return {make_error(sec::logic_error, "SSL_new returned null")};
  }
}

expected<connection> context::new_connection(stream_socket fd,
                                             close_on_shutdown_t) {
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

bool context::enable_default_verify_paths() {
  ERR_clear_error();
  return SSL_CTX_set_default_verify_paths(native(pimpl_)) == 1;
}

bool context::add_verify_path(const char* path) {
  ERR_clear_error();
  return SSL_CTX_load_verify_locations(native(pimpl_), nullptr, path) == 1;
}

bool context::load_verify_file(const char* path) {
  ERR_clear_error();
  return SSL_CTX_load_verify_locations(native(pimpl_), path, nullptr) == 1;
}

bool context::use_certificate_file(const char* path, format file_format) {
  ERR_clear_error();
  auto nff = native(file_format);
  return SSL_CTX_use_certificate_file(native(pimpl_), path, nff) == 1;
}

bool context::use_certificate_chain_file(const char* path) {
  ERR_clear_error();
  return SSL_CTX_use_certificate_chain_file(native(pimpl_), path) == 1;
}

bool context::use_private_key_file(const char* path, format file_format) {
  ERR_clear_error();
  auto nff = native(file_format);
  return SSL_CTX_use_PrivateKey_file(native(pimpl_), path, nff) == 1;
}

} // namespace caf::net::ssl
