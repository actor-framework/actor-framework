// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.web_socket.client

#include "caf/net/web_socket/client.hpp"

#include "net-test.hpp"

using namespace caf;

namespace {

using svec = std::vector<std::string>;

constexpr std::string_view key = "the sample nonce";

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

auto key_to_bytes() {
  net::web_socket::handshake::key_type bytes;
  auto in = as_bytes(make_span(key));
  std::copy(in.begin(), in.end(), bytes.begin());
  return bytes;
}

auto make_handshake() {
  auto result = std::make_unique<net::web_socket::handshake>();
  result->endpoint("/chat");
  result->host("server.example.com");
  result->key(key_to_bytes());
  result->origin("http://example.com");
  result->protocols("chat, superchat");
  return result;
}

} // namespace

SCENARIO("the client performs the WebSocket handshake on startup") {
  GIVEN("valid WebSocket handshake data") {
    WHEN("starting a WebSocket client") {
      auto app = mock_web_socket_app::make();
      auto ws = net::web_socket::client::make(make_handshake(), std::move(app));
      auto uut = mock_stream_transport::make(std::move(ws));
      THEN("the client sends its HTTP request when initializing it") {
        CHECK_EQ(uut->start(nullptr), error{});
        CHECK_EQ(uut->output_as_str(), http_request);
      }
      AND("the client waits for the server handshake and validates it") {
        uut->push(http_response);
        CHECK_EQ(uut->handle_input(),
                 static_cast<ptrdiff_t>(http_response.size()));
      }
    }
  }
}
