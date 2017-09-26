/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/openssl/session.hpp"

#include <cassert>

CAF_PUSH_WARNINGS
#include <sys/socket.h>
#include <openssl/err.h>
#include <openssl/bio.h>
CAF_POP_WARNINGS

#include "caf/actor_system_config.hpp"

#include "caf/io/network/default_multiplexer.hpp"

#include "caf/openssl/manager.hpp"

namespace caf {
namespace openssl {

namespace {

int pem_passwd_cb(char* buf, int size, int, void* ptr) {
  auto passphrase = reinterpret_cast<session*>(ptr)->openssl_passphrase();
  strncpy(buf, passphrase, static_cast<size_t>(size));
  buf[size - 1] = '\0';
  return static_cast<int>(strlen(buf));
}

#if 1
//#ifdef CAF_LINUX

// -- custom BIO for avoiding SIGPIPE events

int caf_bio_write(BIO* self, const char* buf, int len) {
  using namespace caf::io::network;
  assert(len > 0);
  BIO_clear_retry_flags(self);
  int fd = 0;
  BIO_get_fd(self, &fd);
  auto res = ::send(fd, buf, len, no_sigpipe_io_flag);
  if (res <= 0 && !is_error(res, true))
    BIO_set_retry_write(self);
  return res;
}

int caf_bio_write_ex(BIO* self, const char* buf, size_t len, size_t* out) {
  len = std::min(len, static_cast<size_t>(std::numeric_limits<int>::max()));
  auto res = caf_bio_write(self, buf, static_cast<int>(len));
  if (res <= 0) {
    *out = 0;
    return res;
  }
  *out = static_cast<size_t>(res);
  return 1;
}

int caf_bio_read(BIO* self, char* buf, int len) {
  using namespace caf::io::network;
  assert(len > 0);
  BIO_clear_retry_flags(self);
  int fd = 0;
  BIO_get_fd(self, &fd);
  auto res = ::recv(fd, buf, len, no_sigpipe_io_flag);
  if (res <= 0 && !is_error(res, true))
    BIO_set_retry_read(self);
  return res;
}

int caf_bio_read_ex(BIO* self, char* buf, size_t len, size_t* out) {
  len = std::min(len, static_cast<size_t>(std::numeric_limits<int>::max()));
  auto res = caf_bio_read(self, buf, static_cast<int>(len));
  if (res <= 0) {
    *out= 0;
    return res;
  }
  *out = static_cast<size_t>(res);
  return 1;
}

int caf_bio_puts(BIO* self, const char* cstr) {
  return caf_bio_write(self, cstr, static_cast<int>(strlen(cstr)));
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L // OpenSSL < 1.1

long caf_bio_ctrl(BIO* self, int cmd, long num, void* ptr) {
  switch (cmd) {
    case BIO_C_SET_FD: {
      auto fd = *reinterpret_cast<int*>(ptr);
      self->num = fd;
      self->shutdown = fd;
      self->init = 1;
      return 1;
    }
    case BIO_C_GET_FD:
      if (self->init != 0) {
        if (ptr != nullptr)
          *reinterpret_cast<int*>(ptr) = self->num;
        return self->num;
      }
      return -1;
    case BIO_CTRL_GET_CLOSE:
      return self->shutdown;
    case BIO_CTRL_SET_CLOSE:
      self->shutdown = static_cast<int>(num);
      return 1;
      break;
    case BIO_CTRL_DUP:
    case BIO_CTRL_FLUSH:
      return 1;
    default:
      return 0;
  }
}

int caf_bio_create(BIO* self) {
  self->init = 0;
  self->num = 0;
  self->ptr = nullptr;
  BIO_clear_flags(self, 0);
  return 1;
}

int caf_bio_destroy(BIO* self) {
  return self == nullptr ? 0 : 1;
}

BIO_METHOD caf_bio_method = {
  BIO_TYPE_SOCKET,
  "CAFsocket",
  caf_bio_write,
  caf_bio_read,
  caf_bio_puts,
  nullptr, // gets
  caf_bio_ctrl,
  caf_bio_create,
  caf_bio_destroy,
  nullptr
};

BIO_METHOD* new_caf_bio() {
  return &caf_bio_method;
}

inline void delete_caf_bio(BIO_METHOD*) {
  // nop
}

#else // OpenSSL >= 1.1

BIO_METHOD* new_caf_bio() {
  auto cs = const_cast<BIO_METHOD*>(BIO_s_socket());
  auto ptr = BIO_meth_new(BIO_TYPE_SOCKET, "CAFsocket");
  BIO_meth_set_write(ptr, caf_bio_write);
  BIO_meth_set_write_ex(ptr, caf_bio_write_ex);
  BIO_meth_set_read(ptr, caf_bio_read);
  BIO_meth_set_read_ex(ptr, caf_bio_read_ex);
  BIO_meth_set_puts(ptr, caf_bio_puts);
  BIO_meth_set_ctrl(ptr, BIO_meth_get_ctrl(cs));
  BIO_meth_set_create(ptr, BIO_meth_get_create(cs));
  BIO_meth_set_destroy(ptr, BIO_meth_get_destroy(cs));
  return ptr;
}

inline void delete_caf_bio(BIO_METHOD* self) {
  BIO_meth_free(self);
}

#endif // OpenSSL 1.1

#else

inline BIO_METHOD* new_caf_bio() {
  return BIO_s_socket();
}

inline void delete_caf_bio(BIO_METHOD*) {
  // nop
}

#endif

} // namespace <anonymous>

session::session(actor_system& sys)
    : sys_(sys),
      biom_(new_caf_bio()),
      ctx_(nullptr),
      ssl_(nullptr),
      connecting_(false),
      accepting_(false) {
  // nop
}

bool session::init() {
  CAF_LOG_TRACE("");
  ctx_ = create_ssl_context();
  ssl_ = SSL_new(ctx_);
  if (ssl_ == nullptr) {
    CAF_LOG_ERROR("cannot create SSL session");
    return false;
  }
  return true;
}

session::~session() {
  SSL_free(ssl_);
  SSL_CTX_free(ctx_);
  delete_caf_bio(biom_);
}

rw_state session::do_some(int (*f)(SSL*, void*, int), size_t& result, void* buf,
                          size_t len, const char* debug_name) {
  auto check_ssl_res = [&](int res) -> rw_state {
    result = 0;
    switch (SSL_get_error(ssl_, res)) {
      default:
        CAF_LOG_INFO("SSL error:" << get_ssl_error());
        return rw_state::failure;
      case SSL_ERROR_WANT_READ:
        CAF_LOG_DEBUG("SSL_ERROR_WANT_READ reported");
        // Report success to poll on this socket.
        if (len == 0 && strcmp(debug_name, "write_some") == 0)
          return rw_state::indeterminate;
        return rw_state::success;
      case SSL_ERROR_WANT_WRITE:
        CAF_LOG_DEBUG("SSL_ERROR_WANT_WRITE reported");
        // Report success to poll on this socket.
        return rw_state::success;
    }
  };
  CAF_LOG_TRACE(CAF_ARG(len) << CAF_ARG(debug_name));
  CAF_IGNORE_UNUSED(debug_name);
  if (connecting_) {
    CAF_LOG_DEBUG(debug_name << ": connecting");
    auto res = SSL_connect(ssl_);
    if (res == 1) {
      CAF_LOG_DEBUG("SSL connection established");
      connecting_ = false;
    } else {
      result = 0;
      return check_ssl_res(res);
    }
  }
  if (accepting_) {
    CAF_LOG_DEBUG(debug_name << ": accepting");
    auto res = SSL_accept(ssl_);
    if (res == 1) {
      CAF_LOG_DEBUG("SSL connection accepted");
      accepting_ = false;
    } else {
      result = 0;
      return check_ssl_res(res);
    }
  }
  CAF_LOG_DEBUG(debug_name << ": calling SSL_write or SSL_read");
  if (len == 0) {
    result = 0;
    return rw_state::indeterminate;
  }
  auto ret = f(ssl_, buf, static_cast<int>(len));
  if (ret > 0) {
    result = static_cast<size_t>(ret);
    return rw_state::success;
  }
  result = 0;
  return handle_ssl_result(ret) ? rw_state::success : rw_state::failure;
}

rw_state session::read_some(size_t& result, native_socket, void* buf,
                            size_t len) {
  CAF_LOG_TRACE(CAF_ARG(len));
  return do_some(SSL_read, result, buf, len, "read_some");
}

rw_state session::write_some(size_t& result, native_socket, const void* buf,
                             size_t len) {
  CAF_LOG_TRACE(CAF_ARG(len));
  auto wr_fun = [](SSL* sptr, void* vptr, int ptr_size) {
    return SSL_write(sptr, vptr, ptr_size);
  };
  return do_some(wr_fun, result, const_cast<void*>(buf), len, "write_some");
}

bool session::try_connect(native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  auto bio = BIO_new(biom_);
  if (!bio)
    return false;
  BIO_set_fd(bio, fd, BIO_NOCLOSE);
  SSL_set_bio(ssl_, bio, bio);
  //SSL_set_fd(ssl_, fd);
  SSL_set_connect_state(ssl_);
  auto ret = SSL_connect(ssl_);
  if (ret == 1)
    return true;
  connecting_ = true;
  return handle_ssl_result(ret);
}

bool session::try_accept(native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  auto bio = BIO_new(biom_);
  if (!bio)
    return false;
  BIO_set_fd(bio, fd, BIO_NOCLOSE);
  SSL_set_bio(ssl_, bio, bio);
  //SSL_set_fd(ssl_, fd);
  SSL_set_accept_state(ssl_);
  auto ret = SSL_accept(ssl_);
  if (ret == 1)
    return true;
  accepting_ = true;
  return handle_ssl_result(ret);
}

const char* session::openssl_passphrase() {
  return openssl_passphrase_.c_str();
}

SSL_CTX* session::create_ssl_context() {
  auto ctx = SSL_CTX_new(TLSv1_2_method());
  if (!ctx)
    raise_ssl_error("cannot create OpenSSL context");
  if (sys_.openssl_manager().authentication_enabled()) {
    // Require valid certificates on both sides.
    auto& cfg = sys_.config();
    if (cfg.openssl_certificate.size() > 0
        && SSL_CTX_use_certificate_chain_file(ctx,
                                              cfg.openssl_certificate.c_str())
             != 1)
      raise_ssl_error("cannot load certificate");
    if (cfg.openssl_passphrase.size() > 0) {
      openssl_passphrase_ = cfg.openssl_passphrase;
      SSL_CTX_set_default_passwd_cb(ctx, pem_passwd_cb);
      SSL_CTX_set_default_passwd_cb_userdata(ctx, this);
    }
    if (cfg.openssl_key.size() > 0
        && SSL_CTX_use_PrivateKey_file(ctx, cfg.openssl_key.c_str(),
                                       SSL_FILETYPE_PEM)
             != 1)
      raise_ssl_error("cannot load private key");
    auto cafile =
      (cfg.openssl_cafile.size() > 0 ? cfg.openssl_cafile.c_str() : nullptr);
    auto capath =
      (cfg.openssl_capath.size() > 0 ? cfg.openssl_capath.c_str() : nullptr);
    if (cafile || capath) {
      if (SSL_CTX_load_verify_locations(ctx, cafile, capath) != 1)
        raise_ssl_error("cannot load trusted CA certificates");
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       nullptr);
    if (SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!MD5") != 1)
      raise_ssl_error("cannot set cipher list");
  } else {
    // No authentication.
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    auto ecdh = EC_KEY_new_by_curve_name(NID_secp384r1);
    if (!ecdh)
      raise_ssl_error("cannot get ECDH curve");
    CAF_PUSH_WARNINGS
    SSL_CTX_set_tmp_ecdh(ctx, ecdh);
    CAF_POP_WARNINGS
    if (SSL_CTX_set_cipher_list(ctx, "AECDH-AES256-SHA") != 1)
      raise_ssl_error("cannot set anonymous cipher");
  }
  return ctx;
}

std::string session::get_ssl_error() {
  std::string msg = "";
  while (auto err = ERR_get_error()) {
    if (msg.size() > 0)
      msg += " ";
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    msg += buf;
  }
  return msg;
}

void session::raise_ssl_error(std::string msg) {
  CAF_RAISE_ERROR(std::string("[OpenSSL] ") + msg + ": " + get_ssl_error());
}

bool session::handle_ssl_result(int ret) {
  auto err = SSL_get_error(ssl_, ret);
  switch (err) {
    case SSL_ERROR_WANT_READ:
      CAF_LOG_DEBUG("Nonblocking call to SSL returned want_read");
      return true;
    case SSL_ERROR_WANT_WRITE:
      CAF_LOG_DEBUG("Nonblocking call to SSL returned want_write");
      return true;
    case SSL_ERROR_ZERO_RETURN: // Regular remote connection shutdown.
    case SSL_ERROR_SYSCALL:     // Socket connection closed.
      return false;
    default: // Other error
      CAF_LOG_INFO("SSL call failed:" << get_ssl_error());
      return false;
  }
}

session_ptr make_session(actor_system& sys, native_socket fd,
                         bool from_accepted_socket) {
  session_ptr ptr{new session(sys)};
  if (!ptr->init())
    return nullptr;
  if (from_accepted_socket) {
    if (!ptr->try_accept(fd))
      return nullptr;
  } else {
    if (!ptr->try_connect(fd))
      return nullptr;
  }
  return ptr;
}

} // namespace openssl
} // namespace caf
