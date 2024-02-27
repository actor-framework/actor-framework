// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/client.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/http/v1.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/web_socket/framing.hpp"
#include "caf/net/web_socket/handshake.hpp"

#include "caf/byte_span.hpp"
#include "caf/detail/base64.hpp"
#include "caf/detail/message_flow_bridge.hpp"
#include "caf/error.hpp"
#include "caf/hash/sha1.hpp"
#include "caf/log/net.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"

#include <algorithm>

namespace caf::net::web_socket {

// -- factories ----------------------------------------------------------------

std::unique_ptr<client> client::make(handshake_ptr hs, upper_layer_ptr up_ptr) {
  return std::make_unique<client>(std::move(hs), std::move(up_ptr));
}

// -- implementation of octet_stream::upper_layer ------------------------------

error client::start(octet_stream::lower_layer* down) {
  CAF_ASSERT(hs_ != nullptr);
  if (!hs_->has_mandatory_fields())
    return make_error(sec::runtime_error,
                      "WebSocket client received an incomplete handshake");
  if (!hs_->has_valid_key())
    hs_->randomize_key();
  down_ = down;
  down_->begin_output();
  hs_->write_http_1_request(down_->output_buffer());
  down_->end_output();
  down_->configure_read(receive_policy::up_to(handshake::max_http_size));
  return none;
}

void client::abort(const error& reason) {
  CAF_ASSERT(up_);
  up_->abort(reason);
}

ptrdiff_t client::consume(byte_span buffer, byte_span) {
  auto exit_guard = log::net::trace("buffer = {}", buffer.size());
  // Check whether we have received the HTTP header or else wait for more
  // data. Abort when exceeding the maximum size.
  auto [hdr, remainder] = http::v1::split_header(buffer);
  if (hdr.empty()) {
    if (buffer.size() >= handshake::max_http_size) {
      log::net::error("server response exceeded the maximum header size");
      up_->abort(make_error(sec::protocol_error, "server response exceeded "
                                                 "the maximum header size"));
      return -1;
    }
    // Wait for more data.
    return 0;
  }
  if (!handle_header(hdr)) {
    // Note: handle_header() already calls upper_layer().abort().
    return -1;
  }
  // We only care about the header here. The framing layer is responsible for
  // any remaining data.
  return static_cast<ptrdiff_t>(hdr.size());
}

void client::prepare_send() {
  // nop
}

bool client::done_sending() {
  return true;
}

// -- HTTP response processing -------------------------------------------------

bool client::handle_header(std::string_view http) {
  CAF_ASSERT(hs_ != nullptr);
  auto http_ok = hs_->is_valid_http_1_response(http);
  hs_.reset();
  if (http_ok) {
    down_->switch_protocol(framing::make_client(std::move(up_)));
    return true;
  }
  log::net::debug("received an invalid WebSocket handshake");
  up_->abort(
    make_error(sec::protocol_error, "received an invalid WebSocket handshake"));
  return false;
}

} // namespace caf::net::web_socket
