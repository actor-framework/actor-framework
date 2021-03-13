// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.web_socket.client

#include "caf/net/web_socket/client.hpp"

#include "net-test.hpp"

using namespace caf;
using namespace caf::literals;
using namespace std::literals::string_literals;

namespace {

using svec = std::vector<std::string>;

struct app_t {
  std::string text_input;

  std::vector<byte> binary_input;

  settings cfg;

  template <class LowerLayerPtr>
  error init(net::socket_manager*, LowerLayerPtr, const settings& init_cfg) {
    cfg = init_cfg;
    return none;
  }

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr) {
    return true;
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr) {
    return true;
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr, const error& reason) {
    CAF_FAIL("app::abort called: " << reason);
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume_text(LowerLayerPtr, string_view text) {
    text_input.insert(text_input.end(), text.begin(), text.end());
    return static_cast<ptrdiff_t>(text.size());
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume_binary(LowerLayerPtr, byte_span bytes) {
    binary_input.insert(binary_input.end(), bytes.begin(), bytes.end());
    return static_cast<ptrdiff_t>(bytes.size());
  }
};

constexpr auto key = "the sample nonce"_sv;

constexpr auto http_request
  = "GET /chat HTTP/1.1\r\n"
    "Host: server.example.com\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Origin: http://example.com\r\n"
    "Sec-WebSocket-Protocol: chat, superchat\r\n"
    "\r\n"_sv;

constexpr auto http_response
  = "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
    "\r\n"_sv;

auto key_to_bytes() {
  net::web_socket::handshake::key_type bytes;
  auto in = as_bytes(make_span(key));
  std::copy(in.begin(), in.end(), bytes.begin());
  return bytes;
}

auto make_handshake() {
  net::web_socket::handshake result;
  result.endpoint("/chat");
  result.host("server.example.com");
  result.key(key_to_bytes());
  result.origin("http://example.com");
  result.protocols("chat, superchat");
  return result;
}

using mock_client_type = mock_stream_transport<net::web_socket::client<app_t>>;

} // namespace

BEGIN_FIXTURE_SCOPE(host_fixture)

SCENARIO("the client performs the WebSocket handshake on startup") {
  GIVEN("valid WebSocket handshake data") {
    WHEN("starting a WebSocket client"){
      mock_client_type client{make_handshake()};
      THEN("the client sends its HTTP request when initializing it") {
        CHECK_EQ(client.init(), error{});
        CHECK_EQ(client.output_as_str(), http_request);
      }
      AND("the client waits for the server handshake and validates it") {
        client.push(http_response);
        CHECK_EQ(client.handle_input(),
                 static_cast<ptrdiff_t>(http_response.size()));
        CHECK(client.upper_layer.handshake_complete());
      }
    }
  }
}

END_FIXTURE_SCOPE()
