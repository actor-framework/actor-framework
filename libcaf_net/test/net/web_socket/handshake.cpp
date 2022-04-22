// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.web_socket.handshake

#include "caf/net/web_socket/handshake.hpp"

#include "net-test.hpp"

#include <algorithm>
#include <string_view>

using namespace std::literals;

using namespace caf::net;
using namespace caf;

namespace {

constexpr auto key = "the sample nonce"sv;

constexpr std::string_view http_request
  = "GET /chat HTTP/1.1\r\n"
    "Host: server.example.com\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Origin: http://example.com\r\n"
    "Sec-WebSocket-Protocol: chat, superchat\r\n"
    "\r\n";

constexpr std::string_view http_response
  = "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
    "\r\n";

struct fixture {
  byte_buffer& buf() {
    return bytes;
  }

  std::string_view str() {
    return {reinterpret_cast<char*>(bytes.data()), bytes.size()};
  }

  auto key_to_bytes() {
    web_socket::handshake::key_type bytes;
    auto in = as_bytes(make_span(key));
    std::copy(in.begin(), in.end(), bytes.begin());
    return bytes;
  }

  byte_buffer bytes;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("handshake generates HTTP GET requests according to RFC 6455") {
  GIVEN("a request header object with endpoint, origin and protocol") {
    web_socket::handshake uut;
    uut.endpoint("/chat");
    uut.host("server.example.com");
    uut.key(key_to_bytes());
    uut.origin("http://example.com");
    uut.protocols("chat, superchat");
    WHEN("when generating the HTTP handshake") {
      THEN("CAF creates output according to RFC 6455 and omits empty fields") {
        uut.write_http_1_request(buf());
        CHECK_EQ(str(), http_request);
      }
    }
  }
}

SCENARIO("handshake objects validate HTTP response headers") {
  GIVEN("a request header object with predefined key") {
    web_socket::handshake uut;
    uut.endpoint("/chat");
    uut.key(key_to_bytes());
    WHEN("presenting a HTTP response with proper Sec-WebSocket-Accept") {
      THEN("the object recognizes the response as valid") {
        CHECK(uut.is_valid_http_1_response(http_response));
        CHECK(!uut.is_valid_http_1_response("HTTP/1.1 101 Bogus\r\n"));
      }
    }
  }
}

END_FIXTURE_SCOPE()
