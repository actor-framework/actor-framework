// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/ssl/connection.hpp"

#include "caf/test/test.hpp"

#include "caf/net/socket_guard.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/ssl/tls.hpp"
#include "caf/net/stream_socket.hpp"

CAF_PUSH_WARNINGS
#include <openssl/crypto.h>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
CAF_POP_WARNINGS

#include <memory>
#include <source_location>
#include <string>
#include <string_view>

namespace ssl = caf::net::ssl;

using namespace caf;

namespace {

// OpenSSL 3.0 added X509_VERIFY_PARAM_get0_host and
// X509_VERIFY_PARAM_get1_ip_asc, which let us inspect the configured identity.
// OpenSSL 1.1.1 has no public API for reading these values, so these tests are
// only compiled for OpenSSL 3.0 and newer.
#if OPENSSL_VERSION_NUMBER >= 0x30000000L

ssl::connection make_connection() {
  auto& this_test = test::runnable::current();
  auto maybe_ctx = ssl::context::make_client(ssl::tls::v1_2);
  this_test.require_has_value(maybe_ctx);
  auto* ctx = static_cast<SSL_CTX*>(maybe_ctx->native_handle());
  auto* native = SSL_new(ctx);
  this_test.require(native != nullptr);
  return ssl::connection::from_native(native);
}

struct connection_fixture {
  ssl::connection conn = make_connection();

  void check_hostname(std::string_view expected,
                      const std::source_location& location
                      = std::source_location::current()) {
    auto& this_test = test::runnable::current();
    auto* native = static_cast<SSL*>(conn.native_handle());
    auto* params = SSL_get0_param(native);
    auto* actual = X509_VERIFY_PARAM_get0_host(params, 0);
    if (expected.empty()) {
      this_test.check_eq(actual, nullptr, location);
    } else if (this_test.check_ne(actual, nullptr, location)) {
      this_test.check_eq(std::string_view{actual}, expected, location);
    }
  }

  void check_ip(std::string_view expected, const std::source_location& location
                                           = std::source_location::current()) {
    auto& this_test = test::runnable::current();
    auto* native = static_cast<SSL*>(conn.native_handle());
    auto* params = SSL_get0_param(native);
    auto* actual = X509_VERIFY_PARAM_get1_ip_asc(params);
    using ip_ptr
      = std::unique_ptr<ASN1_OCTET_STRING, decltype(&ASN1_OCTET_STRING_free)>;
    if (expected.empty()) {
      this_test.check_eq(actual, nullptr, location);
    } else if (this_test.check_ne(actual, nullptr, location)) {
      // OpenSSL may return the address in a canonical format that differs from
      // the input (e.g. uppercase IPv6). Parse both sides before comparing to
      // avoid depending on the exact textual representation.
      auto expected_str = std::string{expected};
      auto actual_ip = ip_ptr{a2i_IPADDRESS(actual), ASN1_OCTET_STRING_free};
      auto expected_ip = ip_ptr{a2i_IPADDRESS(expected_str.c_str()),
                                ASN1_OCTET_STRING_free};
      if (this_test.check(actual_ip != nullptr, location)
          && this_test.check(expected_ip != nullptr, location))
        this_test.check_eq(ASN1_STRING_cmp(actual_ip.get(), expected_ip.get()),
                           0, location);
    }
    OPENSSL_free(actual);
  }
};

WITH_FIXTURE(connection_fixture) {

TEST("setting an identity configures verification parameters") {
  SECTION("DNS name") {
    check(conn.hostname("example.com"));
    check_hostname("example.com");
    check_ip("");
  }
  SECTION("IPv4 address") {
    check(conn.hostname("192.0.2.1"));
    check_hostname("");
    check_ip("192.0.2.1");
  }
  SECTION("IPv6 address") {
    check(conn.hostname("2001:db8::1"));
    check_hostname("");
    check_ip("2001:db8::1");
  }
}

TEST("changing an identity replaces the previous verification parameters") {
  SECTION("IPv4 to IPv6") {
    check(conn.hostname("192.0.2.1"));
    check(conn.hostname("2001:db8::1"));
    check_hostname("");
    check_ip("2001:db8::1");
  }
  SECTION("DNS to IPv4") {
    check(conn.hostname("example.com"));
    check(conn.hostname("192.0.2.1"));
    check_hostname("");
    check_ip("192.0.2.1");
  }
  SECTION("IPv4 to DNS") {
    check(conn.hostname("192.0.2.1"));
    check(conn.hostname("example.com"));
    check_hostname("example.com");
    check_ip("");
  }
  SECTION("DNS to IPv6") {
    check(conn.hostname("example.com"));
    check(conn.hostname("2001:db8::1"));
    check_hostname("");
    check_ip("2001:db8::1");
  }
  SECTION("IPv6 to DNS") {
    check(conn.hostname("2001:db8::1"));
    check(conn.hostname("example.com"));
    check_hostname("example.com");
    check_ip("");
  }
}

TEST("clearing an identity with nullptr removes verification parameters") {
  SECTION("after DNS") {
    check(conn.hostname("example.com"));
    check(conn.hostname(nullptr));
    check_hostname("");
    check_ip("");
  }
  SECTION("after IPv4") {
    check(conn.hostname("192.0.2.1"));
    check(conn.hostname(nullptr));
    check_hostname("");
    check_ip("");
  }
  SECTION("after IPv6") {
    check(conn.hostname("2001:db8::1"));
    check(conn.hostname(nullptr));
    check_hostname("");
    check_ip("");
  }
}

} // WITH_FIXTURE(connection_fixture)

#endif // OPENSSL_VERSION_NUMBER >= 0x30000000L

TEST("rejects ambiguous IP address spellings") {
  auto sockets = net::make_stream_socket_pair();
  require_has_value(sockets);
  auto client = net::make_socket_guard(sockets->first);
  auto peer = net::make_socket_guard(sockets->second);
  auto ctx = ssl::context::make_client(ssl::tls::v1_2);
  require_has_value(ctx);
  auto conn = ctx->new_connection(client.get());
  require_has_value(conn);
  // Resolvers and OpenSSL versions may interpret non-canonical IPv4 spellings
  // differently. This is system and version dependent. In CAF, we're on the
  // defensive side - rejecting them before the SSL layer.
  check(!conn->hostname("127.0.0.001"));
  check(conn->hostname("127.0.0.1"));
}

} // namespace
