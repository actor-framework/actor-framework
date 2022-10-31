#include "caf/net/ssl/verify.hpp"

#include <openssl/ssl.h>

namespace caf::net::ssl::verify {

namespace {

constexpr verify_t i2v(int x) {
  return static_cast<verify_t>(x);
}

} // namespace

const verify_t none = i2v(SSL_VERIFY_NONE);

const verify_t peer = i2v(SSL_VERIFY_PEER);

const verify_t fail_if_no_peer_cert = i2v(SSL_VERIFY_FAIL_IF_NO_PEER_CERT);

const verify_t client_once = i2v(SSL_VERIFY_CLIENT_ONCE);

} // namespace caf::net::ssl::verify
