// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/server.hpp"

namespace caf::net::web_socket {

// -- constructors, destructors, and assignment operators ----------------------

server::server(upper_layer_ptr up) : framing_(std::move(up)) {
  // > A server MUST NOT mask any frames that it sends to the client.
  // See RFC 6455, Section 5.1.
  framing_.mask_outgoing_frames = false;
}

// -- factories ----------------------------------------------------------------

std::unique_ptr<server> server::make(upper_layer_ptr up) {
  return std::make_unique<server>(std::move(up));
}

// -- stream_oriented::upper_layer implementation ------------------------------

error server::start(stream_oriented::lower_layer* down, const settings& cfg) {
  framing_.start(down);
  cfg_ = cfg;
  lower_layer().configure_read(receive_policy::up_to(handshake::max_http_size));
  return none;
}

void server::abort(const error& reason) {
  if (handshake_complete_)
    upper_layer().abort(reason);
}

ptrdiff_t server::consume(byte_span input, byte_span delta) {
  using namespace std::literals;
  CAF_LOG_TRACE(CAF_ARG2("bytes.size", input.size()));
  if (handshake_complete_) {
    // Short circuit to the framing layer after the handshake completed.
    return framing_.consume(input, delta);
  } else {
    // Check whether we received an HTTP header or else wait for more data.
    // Abort when exceeding the maximum size.
    auto [hdr, remainder] = http::v1::split_header(input);
    if (hdr.empty()) {
      if (input.size() >= handshake::max_http_size) {
        lower_layer().begin_output();
        http::v1::write_response(http::status::request_header_fields_too_large,
                                 "text/plain"sv,
                                 "Header exceeds maximum size."sv,
                                 lower_layer().output_buffer());
        lower_layer().end_output();
        return -1;
      } else {
        return 0;
      }
    } else if (!handle_header(hdr)) {
      return -1;
    } else {
      return hdr.size();
    }
  }
}

void server::prepare_send() {
  if (handshake_complete_)
    upper_layer().prepare_send();
}

bool server::done_sending() {
  return handshake_complete_ ? upper_layer().done_sending() : true;
}

// -- HTTP request processing ------------------------------------------------

void server::write_response(http::status code, std::string_view msg) {
  lower_layer().begin_output();
  http::v1::write_response(code, "text/plain", msg,
                           lower_layer().output_buffer());
  lower_layer().end_output();
}

bool server::handle_header(std::string_view http) {
  using namespace std::literals;
  // Parse the header and reject invalid inputs.
  http::header hdr;
  auto [code, msg] = hdr.parse(http);
  if (code != http::status::ok) {
    write_response(code, msg);
    return false;
  }
  if (hdr.method() != http::method::get) {
    write_response(http::status::bad_request,
                   "Expected a WebSocket handshake.");
    return false;
  }
  // Check whether the mandatory fields exist.
  auto sec_key = hdr.field("Sec-WebSocket-Key");
  if (sec_key.empty()) {
    auto descr = "Mandatory field Sec-WebSocket-Key missing or invalid."s;
    write_response(http::status::bad_request, descr);
    CAF_LOG_DEBUG("received invalid WebSocket handshake");
    return false;
  }
  // Store the request information in the settings for the upper layer.
  auto& ws = cfg_["web-socket"].as_dictionary();
  put(ws, "method", to_rfc_string(hdr.method()));
  put(ws, "path", std::string{hdr.path()});
  put(ws, "query", hdr.query());
  put(ws, "fragment", hdr.fragment());
  put(ws, "http-version", hdr.version());
  if (!hdr.fields().empty()) {
    auto& fields = ws["fields"].as_dictionary();
    for (auto& [key, val] : hdr.fields())
      put(fields, std::string{key}, std::string{val});
  }
  // Try to initialize the upper layer.
  lower_layer().configure_read(receive_policy::stop());
  if (auto err = upper_layer().start(&framing_, cfg_)) {
    auto descr = to_string(err);
    CAF_LOG_DEBUG("upper layer rejected a WebSocket connection:" << descr);
    write_response(http::status::bad_request, descr);
    return false;
  }
  // Finalize the WebSocket handshake.
  handshake hs;
  hs.assign_key(sec_key);
  lower_layer().begin_output();
  hs.write_http_1_response(lower_layer().output_buffer());
  lower_layer().end_output();
  CAF_LOG_DEBUG("completed WebSocket handshake");
  handshake_complete_ = true;
  return true;
}

} // namespace caf::net::web_socket
