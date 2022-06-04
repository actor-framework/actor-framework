// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/client.hpp"

#include "caf/byte_span.hpp"
#include "caf/detail/base64.hpp"
#include "caf/error.hpp"
#include "caf/hash/sha1.hpp"
#include "caf/logger.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/http/v1.hpp"
#include "caf/net/message_flow_bridge.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/stream_oriented.hpp"
#include "caf/net/web_socket/framing.hpp"
#include "caf/net/web_socket/handshake.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"

#include <algorithm>

namespace caf::net::web_socket {

// -- constructors, destructors, and assignment operators ----------------------

client::client(handshake_ptr hs, upper_layer_ptr up)
  : hs_(std::move(hs)), framing_(std::move(up)) {
  // nop
}

// -- factories ----------------------------------------------------------------

std::unique_ptr<client> client::make(handshake_ptr hs, upper_layer_ptr up) {
  return std::make_unique<client>(std::move(hs), std::move(up));
}

// -- implementation of stream_oriented::upper_layer ---------------------------

error client::init(socket_manager* owner, stream_oriented::lower_layer* down,
                   const settings& cfg) {
  CAF_ASSERT(hs_ != nullptr);
  framing_.init(owner, down);
  if (!hs_->has_mandatory_fields())
    return make_error(sec::runtime_error,
                      "handshake data lacks mandatory fields");
  if (!hs_->has_valid_key())
    hs_->randomize_key();
  cfg_ = cfg;
  down->begin_output();
  hs_->write_http_1_request(down->output_buffer());
  down->end_output();
  down->configure_read(receive_policy::up_to(handshake::max_http_size));
  return none;
}

void client::abort(const error& reason) {
  upper_layer().abort(reason);
}

ptrdiff_t client::consume(byte_span buffer, byte_span delta) {
  CAF_LOG_TRACE(CAF_ARG2("buffer", buffer.size()));
  if (handshake_completed()) {
    // Short circuit to the framing layer after the handshake completed.
    return framing_.consume(buffer, delta);
  } else {
    // Check whether we have received the HTTP header or else wait for more
    // data. Abort when exceeding the maximum size.
    auto [hdr, remainder] = http::v1::split_header(buffer);
    if (hdr.empty()) {
      if (buffer.size() >= handshake::max_http_size) {
        CAF_LOG_ERROR("server response exceeded the maximum header size");
        upper_layer().abort(make_error(sec::protocol_error,
                                       "server response exceeded "
                                       "the maximum header size"));
        return -1;
      } else {
        return 0;
      }
    } else if (!handle_header(hdr)) {
      // Note: handle_header() already calls upper_layer().abort().
      return -1;
    } else if (remainder.empty()) {
      CAF_ASSERT(hdr.size() == buffer.size());
      return hdr.size();
    } else {
      CAF_LOG_DEBUG(CAF_ARG2("remainder.size", remainder.size()));
      if (auto res = framing_.consume(remainder, remainder); res >= 0) {
        return hdr.size() + res;
      } else {
        return res;
      }
    }
  }
}

void client::continue_reading() {
  if (handshake_completed())
    upper_layer().continue_reading();
}

bool client::prepare_send() {
  return handshake_completed() ? upper_layer().prepare_send() : true;
}

bool client::done_sending() {
  return handshake_completed() ? upper_layer().done_sending() : true;
}

// -- HTTP response processing -------------------------------------------------

bool client::handle_header(std::string_view http) {
  CAF_ASSERT(hs_ != nullptr);
  auto http_ok = hs_->is_valid_http_1_response(http);
  hs_.reset();
  if (http_ok) {
    if (auto err = upper_layer().init(owner_, &framing_, cfg_)) {
      CAF_LOG_DEBUG("failed to initialize WebSocket framing layer");
      return false;
    } else {
      CAF_LOG_DEBUG("completed WebSocket handshake");
      return true;
    }
  } else {
    CAF_LOG_DEBUG("received an invalid WebSocket handshake");
    upper_layer().abort(make_error(sec::protocol_error,
                                   "received an invalid WebSocket handshake"));
    return false;
  }
}

} // namespace caf::net::web_socket
