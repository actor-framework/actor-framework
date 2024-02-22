// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/server.hpp"

#include "caf/log/net.hpp"

namespace caf::net::web_socket {

// -- octet_stream::upper_layer implementation ---------------------------------

error server::start(octet_stream::lower_layer* down) {
  down_ = down;
  down_->configure_read(receive_policy::up_to(handshake::max_http_size));
  return none;
}

void server::abort(const error& err) {
  up_->abort(err);
}

ptrdiff_t server::consume(byte_span input, byte_span) {
  using namespace std::literals;
  CAF_LOG_TRACE(CAF_ARG2("bytes.size", input.size()));
  // Check whether we received an HTTP header or else wait for more data.
  // Abort when exceeding the maximum size.
  auto [hdr, remainder] = http::v1::split_header(input);
  if (hdr.empty()) {
    if (input.size() >= handshake::max_http_size) {
      down_->begin_output();
      http::v1::write_response(http::status::request_header_fields_too_large,
                               "text/plain"sv, "Header exceeds maximum size."sv,
                               down_->output_buffer());
      down_->end_output();
      return -1;
    } else {
      return 0;
    }
  } else if (!handle_header(hdr)) {
    return -1;
  } else {
    return static_cast<ptrdiff_t>(hdr.size());
  }
}

void server::prepare_send() {
  // nop
}

bool server::done_sending() {
  return true;
}

// -- HTTP request processing ------------------------------------------------

void server::write_response(http::status code, std::string_view msg) {
  down_->begin_output();
  http::v1::write_response(code, "text/plain", msg, down_->output_buffer());
  down_->end_output();
}

bool server::handle_header(std::string_view http) {
  using namespace std::literals;
  // Parse the header and reject invalid inputs.
  http::request_header hdr;
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
    log::net::debug("received invalid WebSocket handshake");
    return false;
  }
  // Kindly ask the upper layer to accept a new WebSocket connection.
  if (auto err = up_->accept(hdr)) {
    write_response(http::status::bad_request, to_string(err));
    return false;
  }
  // Finalize the WebSocket handshake.
  handshake hs;
  hs.assign_key(sec_key);
  down_->begin_output();
  hs.write_http_1_response(down_->output_buffer());
  down_->end_output();
  // All done. Switch to the framing protocol.
  log::net::debug("completed WebSocket handshake");
  down_->switch_protocol(framing::make_server(std::move(up_)));
  return true;
}

} // namespace caf::net::web_socket
