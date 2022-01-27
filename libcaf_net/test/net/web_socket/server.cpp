// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.web_socket.server

#include "caf/net/web_socket/server.hpp"

#include "net-test.hpp"

#include "caf/byte_span.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"

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

struct fixture : host_fixture {
  fixture() {
    using namespace caf::net;
    ws = std::addressof(transport.upper_layer);
    app = std::addressof(ws->upper_layer());
    if (auto err = transport.init())
      CAF_FAIL("failed to initialize mock transport: " << err);
    rng.seed(0xD3ADC0D3);
  }

  void rfc6455_append(uint8_t opcode, span<const byte> bytes,
                      std::vector<byte>& out) {
    std::vector<byte> payload{bytes.begin(), bytes.end()};
    auto key = static_cast<uint32_t>(rng());
    detail::rfc6455::mask_data(key, payload);
    detail::rfc6455::assemble_frame(opcode, key, payload, out);
  }

  void rfc6455_append(span<const byte> bytes, std::vector<byte>& out) {
    rfc6455_append(detail::rfc6455::binary_frame, bytes, out);
  }

  void rfc6455_append(string_view text, std::vector<byte>& out) {
    rfc6455_append(detail::rfc6455::text_frame, as_bytes(make_span(text)), out);
  }

  void push(uint8_t opcode, span<const byte> bytes) {
    std::vector<byte> frame;
    rfc6455_append(opcode, bytes, frame);
    transport.push(frame);
  }

  void push(span<const byte> bytes) {
    push(detail::rfc6455::binary_frame, bytes);
  }

  void push(string_view str) {
    push(detail::rfc6455::text_frame, as_bytes(make_span(str)));
  }

  mock_stream_transport<net::web_socket::server<app_t>> transport;

  net::web_socket::server<app_t>* ws;

  app_t* app;

  std::minstd_rand rng;
};

constexpr auto opening_handshake
  = "GET /chat?room=lounge HTTP/1.1\r\n"
    "Host: server.example.com\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Origin: http://example.com\r\n"
    "Sec-WebSocket-Protocol: chat, superchat\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n"_sv;

} // namespace

#define CHECK_SETTING(key, expected_value)                                     \
  if (CAF_CHECK(holds_alternative<std::string>(app->cfg, key)))                \
    CAF_CHECK_EQUAL(get<std::string>(app->cfg, key), expected_value);

CAF_TEST_FIXTURE_SCOPE(web_socket_tests, fixture)

CAF_TEST(applications receive handshake data via config) {
  transport.push(opening_handshake);
  {
    auto consumed = transport.handle_input();
    if (consumed < 0)
      CAF_FAIL("error handling input: " << transport.abort_reason());
    CAF_CHECK_EQUAL(consumed, static_cast<ptrdiff_t>(opening_handshake.size()));
  }
  CAF_CHECK_EQUAL(transport.input.size(), 0u);
  CAF_CHECK_EQUAL(transport.unconsumed(), 0u);
  CAF_CHECK(ws->handshake_complete());
  CHECK_SETTING("web-socket.method", "GET");
  CHECK_SETTING("web-socket.path", "/chat");
  CHECK_SETTING("web-socket.http-version", "HTTP/1.1");
  CHECK_SETTING("web-socket.fields.Host", "server.example.com");
  CHECK_SETTING("web-socket.fields.Upgrade", "websocket");
  CHECK_SETTING("web-socket.fields.Connection", "Upgrade");
  CHECK_SETTING("web-socket.fields.Origin", "http://example.com");
  CHECK_SETTING("web-socket.fields.Sec-WebSocket-Protocol", "chat, superchat");
  CHECK_SETTING("web-socket.fields.Sec-WebSocket-Version", "13");
  CHECK_SETTING("web-socket.fields.Sec-WebSocket-Key",
                "dGhlIHNhbXBsZSBub25jZQ==");
  using str_map = std::map<std::string, std::string>;
  if (auto query = get_as<str_map>(app->cfg, "web-socket.query");
      CAF_CHECK(query))
    CAF_CHECK_EQUAL(*query, str_map({{"room"s, "lounge"s}}));
}

CAF_TEST(the server responds with an HTTP response on success) {
  transport.push(opening_handshake);
  CAF_CHECK_EQUAL(transport.handle_input(),
                  static_cast<ptrdiff_t>(opening_handshake.size()));
  CAF_CHECK(ws->handshake_complete());
  CAF_CHECK(transport.output_as_str(),
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n");
}

CAF_TEST(handshakes may arrive in chunks) {
  svec bufs;
  size_t chunk_size = opening_handshake.size() / 3;
  auto i = opening_handshake.begin();
  bufs.emplace_back(i, i + chunk_size);
  i += chunk_size;
  bufs.emplace_back(i, i + chunk_size);
  i += chunk_size;
  bufs.emplace_back(i, opening_handshake.end());
  transport.push(bufs[0]);
  CAF_CHECK_EQUAL(transport.handle_input(), 0u);
  CAF_CHECK(!ws->handshake_complete());
  transport.push(bufs[1]);
  CAF_CHECK_EQUAL(transport.handle_input(), 0u);
  CAF_CHECK(!ws->handshake_complete());
  transport.push(bufs[2]);
  CAF_CHECK_EQUAL(static_cast<size_t>(transport.handle_input()),
                  opening_handshake.size());
  CAF_CHECK(ws->handshake_complete());
}

CAF_TEST(data may follow the handshake immediately) {
  auto hs_bytes = as_bytes(make_span(opening_handshake));
  std::vector<byte> buf{hs_bytes.begin(), hs_bytes.end()};
  rfc6455_append("Hello WebSocket!\n"_sv, buf);
  rfc6455_append("Bye WebSocket!\n"_sv, buf);
  transport.push(buf);
  CAF_CHECK_EQUAL(transport.handle_input(), static_cast<ptrdiff_t>(buf.size()));
  CAF_CHECK(ws->handshake_complete());
  CAF_CHECK_EQUAL(app->text_input, "Hello WebSocket!\nBye WebSocket!\n");
}

CAF_TEST(data may arrive later) {
  transport.push(opening_handshake);
  CAF_CHECK_EQUAL(transport.handle_input(),
                  static_cast<ptrdiff_t>(opening_handshake.size()));
  CAF_CHECK(ws->handshake_complete());
  push("Hello WebSocket!\nBye WebSocket!\n"_sv);
  transport.handle_input();
  CAF_CHECK_EQUAL(app->text_input, "Hello WebSocket!\nBye WebSocket!\n");
}

CAF_TEST_FIXTURE_SCOPE_END()
