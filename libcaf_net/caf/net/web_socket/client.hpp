// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/web_socket/handshake.hpp"

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

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<client> make(handshake_ptr hs, upper_layer_ptr up);

  static std::unique_ptr<client> make(handshake&& hs, upper_layer_ptr up) {
    return make(std::make_unique<handshake>(std::move(hs)), std::move(up));
  }
};

} // namespace caf::net::web_socket
