// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/web_socket/switch_protocol.hpp"

#include "caf/internal/ws_flow_bridge.hpp"

namespace caf::detail {

ws_switch_protocol_base::~ws_switch_protocol_base() {
  // nop
}

void ws_switch_protocol_base::do_respond_impl(
  net::http::responder& res,
  std::tuple<bool, pull_t, push_t> (*try_accept)(void*), void* try_accept_arg) {
  namespace http = net::http;
  auto& hdr = res.header();
  // Sanity checking.
  if (!hdr.field_equals(ignore_case, "Connection", "upgrade")
      || !hdr.field_equals(ignore_case, "Upgrade", "websocket")) {
    res.respond(net::http::status::bad_request, "text/plain",
                "Expected a WebSocket client handshake request.");
    return;
  }
  auto sec_key = hdr.field("Sec-WebSocket-Key");
  if (sec_key.empty()) {
    res.respond(net::http::status::bad_request, "text/plain",
                "Mandatory field Sec-WebSocket-Key missing or invalid.");
    return;
  }
  // Prepare the WebSocket handshake.
  net::web_socket::handshake hs;
  if (!hs.assign_key(sec_key)) {
    res.respond(net::http::status::internal_server_error, "text/plain",
                "Invalid WebSocket key.");
    return;
  }
  // Call user-defined on_request callback.
  auto [ok, pull, push] = try_accept(try_accept_arg);
  if (!ok) {
    return;
  }
  // Finalize the WebSocket handshake.
  hs.write_response(res.down());
  // Switch to the WebSocket framing protocol.
  using net::web_socket::framing;
  auto bridge = internal::make_ws_flow_bridge(std::move(pull), std::move(push));
  res.down()->switch_protocol(framing::make_server(std::move(bridge)));
}

} // namespace caf::detail
