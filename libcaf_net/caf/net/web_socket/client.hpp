// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/v1.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/web_socket/framing.hpp"
#include "caf/net/web_socket/handshake.hpp"

#include "caf/byte_span.hpp"
#include "caf/detail/base64.hpp"
#include "caf/detail/message_flow_bridge.hpp"
#include "caf/error.hpp"
#include "caf/hash/sha1.hpp"
#include "caf/logger.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"

#include <algorithm>

namespace caf::net::web_socket {

/// Implements the client part for the WebSocket Protocol as defined in RFC
/// 6455. Initially, the layer performs the WebSocket handshake. Once completed,
/// this layer decodes RFC 6455 frames and forwards binary and text messages to
/// `UpperLayer`.
class CAF_NET_EXPORT client : public octet_stream::upper_layer {
public:
  // -- member types -----------------------------------------------------------

  using handshake_ptr = std::unique_ptr<handshake>;

  using upper_layer_ptr = std::unique_ptr<web_socket::upper_layer>;

  // -- constructors, destructors, and assignment operators --------------------

  client(handshake_ptr hs, upper_layer_ptr up)
    : hs_(std::move(hs)), up_(std::move(up)) {
    // nop
  }

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<client> make(handshake_ptr hs, upper_layer_ptr up);

  static std::unique_ptr<client> make(handshake&& hs, upper_layer_ptr up) {
    return make(std::make_unique<handshake>(std::move(hs)), std::move(up));
  }

  // -- implementation of octet_stream::upper_layer ----------------------------

  error start(octet_stream::lower_layer* down) override;

  void abort(const error& reason) override;

  ptrdiff_t consume(byte_span buffer, byte_span delta) override;

  void prepare_send() override;

  bool done_sending() override;

private:
  // -- HTTP response processing -----------------------------------------------

  bool handle_header(std::string_view http);

  // -- member variables -------------------------------------------------------

  /// Points to the transport layer below.
  octet_stream::lower_layer* down_;

  /// Stores the WebSocket handshake data until the handshake completed.
  handshake_ptr hs_;

  /// Next layer in the processing chain.
  upper_layer_ptr up_;
};

} // namespace caf::net::web_socket
