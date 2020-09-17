/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE net.web_socket

#include "caf/net/web_socket.hpp"

#include "net-test.hpp"

#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"

using namespace caf;
using namespace std::literals::string_literals;

namespace {

using byte_span = span<const byte>;

using svec = std::vector<std::string>;

struct app_t {
  std::vector<std::string> lines;

  settings cfg;

  template <class LowerLayer>
  error init(net::socket_manager*, LowerLayer&, const settings& init_cfg) {
    cfg = init_cfg;
    return none;
  }

  template <class LowerLayer>
  bool prepare_send(LowerLayer&) {
    return true;
  }

  template <class LowerLayer>
  bool done_sending(LowerLayer&) {
    return true;
  }

  template <class LowerLayer>
  void abort(LowerLayer&, const error& reason) {
    CAF_FAIL("app::abort called: " << reason);
  }

  template <class LowerLayer>
  ptrdiff_t consume(LowerLayer& down, byte_span buffer, byte_span) {
    constexpr auto nl = byte{'\n'};
    auto e = buffer.end();
    if (auto i = std::find(buffer.begin(), e, nl); i != e) {
      std::string str;
      auto string_size = static_cast<size_t>(std::distance(buffer.begin(), i));
      str.reserve(string_size);
      auto num_bytes = string_size + 1; // Also consume the newline character.
      std::transform(buffer.begin(), i, std::back_inserter(str),
                     [](byte x) { return static_cast<char>(x); });
      lines.emplace_back(std::move(str));
      return num_bytes + consume(down, buffer.subspan(num_bytes), {});
    }
    return 0;
  }
};

struct fixture : host_fixture {
  fixture() {
    using namespace caf::net;
    ws = std::addressof(transport.upper_layer);
    app = std::addressof(ws->upper_layer());
    if (auto err = transport.init())
      CAF_FAIL("failed to initialize mock transport: " << err);
  }

  mock_stream_transport<net::web_socket<app_t>> transport;

  net::web_socket<app_t>* ws;

  app_t* app;
};

constexpr string_view opening_handshake
  = "GET /chat HTTP/1.1\r\n"
    "Host: server.example.com\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Origin: http://example.com\r\n"
    "Sec-WebSocket-Protocol: chat, superchat\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n";

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
      CAF_FAIL("error handling input: " << transport.abort_reason);
    CAF_CHECK_EQUAL(consumed, static_cast<ptrdiff_t>(opening_handshake.size()));
  }
  CAF_CHECK_EQUAL(transport.input.size(), 0u);
  CAF_CHECK_EQUAL(transport.unconsumed(), 0u);
  CAF_CHECK(ws->handshake_complete());
  CHECK_SETTING("web-socket.method", "GET");
  CHECK_SETTING("web-socket.request-uri", "/chat");
  CHECK_SETTING("web-socket.http-version", "HTTP/1.1");
  CHECK_SETTING("web-socket.fields.Host", "server.example.com");
  CHECK_SETTING("web-socket.fields.Upgrade", "websocket");
  CHECK_SETTING("web-socket.fields.Connection", "Upgrade");
  CHECK_SETTING("web-socket.fields.Origin", "http://example.com");
  CHECK_SETTING("web-socket.fields.Sec-WebSocket-Protocol", "chat, superchat");
  CHECK_SETTING("web-socket.fields.Sec-WebSocket-Version", "13");
  CHECK_SETTING("web-socket.fields.Sec-WebSocket-Key",
                "dGhlIHNhbXBsZSBub25jZQ==");
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
  CAF_CHECK_EQUAL(transport.handle_input(), opening_handshake.size());
  CAF_CHECK(ws->handshake_complete());
}

CAF_TEST(data may follow the handshake immediately) {
  std::string buf{opening_handshake.begin(), opening_handshake.end()};
  buf += "Hello WebSocket!\n";
  buf += "Bye WebSocket!\n";
  transport.push(buf);
  CAF_CHECK_EQUAL(transport.handle_input(), static_cast<ptrdiff_t>(buf.size()));
  CAF_CHECK(ws->handshake_complete());
  CAF_CHECK_EQUAL(app->lines, svec({"Hello WebSocket!", "Bye WebSocket!"}));
}

CAF_TEST(data may arrive later) {
  transport.push(opening_handshake);
  CAF_CHECK_EQUAL(transport.handle_input(),
                  static_cast<ptrdiff_t>(opening_handshake.size()));
  CAF_CHECK(ws->handshake_complete());
  auto buf = "Hello WebSocket!\nBye WebSocket!\n"s;
  transport.push(buf);
  CAF_CHECK_EQUAL(transport.handle_input(), static_cast<ptrdiff_t>(buf.size()));
  CAF_CHECK_EQUAL(app->lines, svec({"Hello WebSocket!", "Bye WebSocket!"}));
}

CAF_TEST_FIXTURE_SCOPE_END()
