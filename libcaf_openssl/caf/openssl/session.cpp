// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/openssl/session.hpp"

CAF_PUSH_WARNINGS
#include <openssl/err.h>
CAF_POP_WARNINGS

#include "caf/io/network/default_multiplexer.hpp"

#include "caf/openssl/manager.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/log/openssl.hpp"
#include "caf/log/system.hpp"

// On Linux we need to block SIGPIPE whenever we access OpenSSL functions.
// Unfortunately there's no sane way to configure OpenSSL properly.
#ifdef CAF_LINUX

#  include "caf/detail/scope_guard.hpp"

#  include <signal.h>

#  define CAF_BLOCK_SIGPIPE()                                                  \
    sigset_t sigpipe_mask;                                                     \
    sigemptyset(&sigpipe_mask);                                                \
    sigaddset(&sigpipe_mask, SIGPIPE);                                         \
    sigset_t saved_mask;                                                       \
    if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) == -1) {        \
      perror("pthread_sigmask");                                               \
      exit(1);                                                                 \
    }                                                                          \
    auto sigpipe_restore_guard = ::caf::detail::scope_guard {                  \
      [&]() noexcept {                                                         \
        struct timespec zerotime = {};                                         \
        sigtimedwait(&sigpipe_mask, 0, &zerotime);                             \
        if (pthread_sigmask(SIG_SETMASK, &saved_mask, 0) == -1) {              \
          perror("pthread_sigmask");                                           \
          exit(1);                                                             \
        }                                                                      \
      }                                                                        \
    }

#else

#  define CAF_BLOCK_SIGPIPE() static_cast<void>(0)

#endif // CAF_LINUX

using namespace std::literals;

namespace caf::openssl {

namespace {

int pem_passwd_cb(char* buf, int size, int, void* ptr) {
  auto passphrase = reinterpret_cast<session*>(ptr)->openssl_passphrase();
  strncpy(buf, passphrase, static_cast<size_t>(size));
  buf[size - 1] = '\0';
  return static_cast<int>(strlen(buf));
}

} // namespace

session::session(actor_system& sys)
  : sys_(sys),
    ctx_(nullptr),
    ssl_(nullptr),
    connecting_(false),
    accepting_(false) {
  // nop
}

bool session::init() {
  auto lg = log::openssl::trace("");
  ctx_ = create_ssl_context();
  ssl_ = SSL_new(ctx_);
  if (ssl_ == nullptr) {
    log::system::error("cannot create SSL session");
    return false;
  }
  return true;
}

session::~session() {
  SSL_free(ssl_);
  SSL_CTX_free(ctx_);
}

rw_state session::do_some(int (*f)(SSL*, void*, int), size_t& result, void* buf,
                          size_t len, const char* debug_name) {
  CAF_BLOCK_SIGPIPE();
  auto check_ssl_res = [&](int res) -> rw_state {
    result = 0;
    switch (SSL_get_error(ssl_, res)) {
      default:
        log::openssl::info("SSL error: {}", get_ssl_error());
        return rw_state::failure;
      case SSL_ERROR_WANT_READ:
        log::openssl::debug("SSL_ERROR_WANT_READ reported");
        return rw_state::want_read;
      case SSL_ERROR_WANT_WRITE:
        log::openssl::debug("SSL_ERROR_WANT_WRITE reported");
        // Report success to poll on this socket.
        return rw_state::success;
    }
  };
  auto lg = log::openssl::trace("len = {}, debug_name = {}", len, debug_name);
  if (connecting_) {
    log::openssl::debug("{} : connecting", debug_name);
    auto res = SSL_connect(ssl_);
    if (res == 1) {
      log::openssl::debug("SSL connection established");
      connecting_ = false;
    } else {
      result = 0;
      return check_ssl_res(res);
    }
  }
  if (accepting_) {
    log::openssl::debug("{} : accepting", debug_name);
    auto res = SSL_accept(ssl_);
    if (res == 1) {
      log::openssl::debug("SSL connection accepted");
      accepting_ = false;
    } else {
      result = 0;
      return check_ssl_res(res);
    }
  }
  log::openssl::debug("{} : calling SSL_write or SSL_read", debug_name);
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
  auto lg = log::openssl::trace("len = {}", len);
  return do_some(SSL_read, result, buf, len, "read_some");
}

rw_state session::write_some(size_t& result, native_socket, const void* buf,
                             size_t len) {
  auto lg = log::openssl::trace("len = {}", len);
  auto wr_fun = [](SSL* sptr, void* vptr, int ptr_size) {
    return SSL_write(sptr, vptr, ptr_size);
  };
  return do_some(wr_fun, result, const_cast<void*>(buf), len, "write_some");
}

bool session::try_connect(native_socket fd) {
  auto lg = log::openssl::trace("fd = {}", fd);
  CAF_BLOCK_SIGPIPE();
  SSL_set_fd(ssl_, fd);
  SSL_set_connect_state(ssl_);
  auto ret = SSL_connect(ssl_);
  if (ret == 1)
    return true;
  connecting_ = true;
  return handle_ssl_result(ret);
}

bool session::try_accept(native_socket fd) {
  auto lg = log::openssl::trace("fd = {}", fd);
  CAF_BLOCK_SIGPIPE();
  SSL_set_fd(ssl_, fd);
  SSL_set_accept_state(ssl_);
  auto ret = SSL_accept(ssl_);
  if (ret == 1)
    return true;
  accepting_ = true;
  return handle_ssl_result(ret);
}

bool session::must_read_more(native_socket, size_t threshold) {
  return static_cast<size_t>(SSL_pending(ssl_)) >= threshold;
}

const char* session::openssl_passphrase() {
  return openssl_passphrase_.c_str();
}

SSL_CTX* session::create_ssl_context() {
  CAF_BLOCK_SIGPIPE();
#ifdef CAF_SSL_HAS_NON_VERSIONED_TLS_FUN
  auto ctx = SSL_CTX_new(TLS_method());
#else
  auto ctx = SSL_CTX_new(TLSv1_2_method());
#endif
  if (!ctx)
    CAF_RAISE_ERROR("cannot create OpenSSL context");
  auto& cfg = sys_.config();
  auto key = get_or(cfg, "caf.openssl.key", ""sv);
  auto certificate = get_or(cfg, "caf.openssl.certificate", ""sv);
  openssl_passphrase_ = get_or(cfg, "caf.openssl.passphrase", ""sv);
  auto capath = get_or(cfg, "caf.openssl.capath", ""sv);
  auto cafile = get_or(cfg, "caf.openssl.cafile", ""sv);
  if (sys_.openssl_manager().authentication_enabled()) {
    // Require valid certificates on both sides.
    if (!certificate.empty()
        && SSL_CTX_use_certificate_chain_file(ctx, certificate.c_str()) != 1)
      CAF_RAISE_ERROR("cannot load certificate");
    if (!openssl_passphrase_.empty()) {
      SSL_CTX_set_default_passwd_cb(ctx, pem_passwd_cb);
      SSL_CTX_set_default_passwd_cb_userdata(ctx, this);
    }
    if (!key.empty()
        && SSL_CTX_use_PrivateKey_file(ctx, key.c_str(), SSL_FILETYPE_PEM) != 1)
      CAF_RAISE_ERROR("cannot load private key");
    auto cafile_cstr = !cafile.empty() ? cafile.c_str() : nullptr;
    auto capath_cstr = !capath.empty() ? capath.c_str() : nullptr;
    if (cafile_cstr || capath_cstr) {
      if (SSL_CTX_load_verify_locations(ctx, cafile_cstr, capath_cstr) != 1)
        CAF_RAISE_ERROR("cannot load trusted CA certificates");
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       nullptr);
  } else {
    // No authentication.
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
#if defined(CAF_SSL_HAS_ECDH_AUTO) && (OPENSSL_VERSION_NUMBER < 0x10100000L)
    SSL_CTX_set_ecdh_auto(ctx, 1);
#else
#  if OPENSSL_VERSION_NUMBER < 0x10101000L
    auto ecdh = EC_KEY_new_by_curve_name(NID_secp384r1);
    if (!ecdh)
      CAF_RAISE_ERROR("cannot get ECDH curve");
    CAF_PUSH_WARNINGS
    SSL_CTX_set_tmp_ecdh(ctx, ecdh);
    EC_KEY_free(ecdh);
    CAF_POP_WARNINGS
#  else  /* OPENSSL_VERSION_NUMBER < 0x10101000L */
    SSL_CTX_set1_groups_list(ctx, "P-384");
#  endif /* OPENSSL_VERSION_NUMBER < 0x10101000L */
#endif
  }
  // Set custom cipher list if specified.
  if (auto str = get_if<std::string>(&cfg, "caf.openssl.cipher-list");
      str && !str->empty()) {
    if (SSL_CTX_set_cipher_list(ctx, str->c_str()) != 1)
      CAF_RAISE_ERROR("failed to set cipher list");
  }
  return ctx;
}

std::string session::get_ssl_error() {
  std::string msg = "";
  while (auto err = ERR_get_error()) {
    if (!msg.empty())
      msg += " ";
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    msg += buf;
  }
  return msg;
}

bool session::handle_ssl_result(int ret) {
  auto err = SSL_get_error(ssl_, ret);
  switch (err) {
    case SSL_ERROR_WANT_READ:
      log::openssl::debug("Nonblocking call to SSL returned want_read");
      return true;
    case SSL_ERROR_WANT_WRITE:
      log::openssl::debug("Nonblocking call to SSL returned want_write");
      return true;
    case SSL_ERROR_ZERO_RETURN: // Regular remote connection shutdown.
    case SSL_ERROR_SYSCALL:     // Socket connection closed.
      return false;
    default: // Other error
      log::openssl::info("SSL call failed: {}", get_ssl_error());
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

} // namespace caf::openssl
