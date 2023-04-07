#include "caf/net/ssl/errc.hpp"

#include "caf/config.hpp"

CAF_PUSH_WARNINGS
#include <openssl/err.h>
#include <openssl/ssl.h>
CAF_POP_WARNINGS

namespace caf::detail {

net::ssl::errc ssl_errc_from_native(int code) {
  using net::ssl::errc;
  switch (code) {
    case SSL_ERROR_NONE:
      return errc::none;
    case SSL_ERROR_ZERO_RETURN:
      return errc::closed;
    case SSL_ERROR_WANT_READ:
      return errc::want_read;
    case SSL_ERROR_WANT_WRITE:
      return errc::want_write;
    case SSL_ERROR_WANT_CONNECT:
      return errc::want_connect;
    case SSL_ERROR_WANT_ACCEPT:
      return errc::want_accept;
    case SSL_ERROR_WANT_X509_LOOKUP:
      return errc::want_x509_lookup;
#ifdef SSL_ERROR_WANT_ASYNC
    case SSL_ERROR_WANT_ASYNC:
      return errc::want_async;
#endif
#ifdef SSL_ERROR_WANT_ASYNC_JOB
    case SSL_ERROR_WANT_ASYNC_JOB:
      return errc::want_async_job;
#endif
#ifdef SSL_ERROR_WANT_CLIENT_HELLO_CB
    case SSL_ERROR_WANT_CLIENT_HELLO_CB:
      return errc::want_client_hello;
#endif
    case SSL_ERROR_SYSCALL:
      return errc::syscall_failed;
    case SSL_ERROR_SSL:
      return errc::fatal;
    default:
      return errc::unspecified;
  }
}

} // namespace caf::detail
