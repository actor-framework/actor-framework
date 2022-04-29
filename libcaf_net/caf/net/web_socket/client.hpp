// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

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

/// Implements the client part for the WebSocket Protocol as defined in RFC
/// 6455. Initially, the layer performs the WebSocket handshake. Once completed,
/// this layer decodes RFC 6455 frames and forwards binary and text messages to
/// `UpperLayer`.
class client : public stream_oriented::upper_layer {
public:
  // -- member types -----------------------------------------------------------

  using handshake_ptr = std::unique_ptr<handshake>;

  using upper_layer_ptr = std::unique_ptr<web_socket::upper_layer>;

  // -- constructors, destructors, and assignment operators --------------------

  client(handshake_ptr hs, upper_layer_ptr up);

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<client> make(handshake_ptr hs, upper_layer_ptr up);

  // -- properties -------------------------------------------------------------

  web_socket::upper_layer& upper_layer() noexcept {
    return framing_.upper_layer();
  }

  const web_socket::upper_layer& upper_layer() const noexcept {
    return framing_.upper_layer();
  }

  stream_oriented::lower_layer& lower_layer() noexcept {
    return framing_.lower_layer();
  }

  const stream_oriented::lower_layer& lower_layer() const noexcept {
    return framing_.lower_layer();
  }

  bool handshake_completed() const noexcept {
    return hs_ == nullptr;
  }

  // -- implementation of stream_oriented::upper_layer -------------------------

  error init(socket_manager* owner, stream_oriented::lower_layer* down,
             const settings& config) override;

  void abort(const error& reason) override;

  ptrdiff_t consume(byte_span buffer, byte_span delta) override;

  bool prepare_send() override;

  bool done_sending() override;

private:
  // -- HTTP response processing -----------------------------------------------

  bool handle_header(std::string_view http);

  // -- member variables -------------------------------------------------------

  /// Stores the WebSocket handshake data until the handshake completed.
  handshake_ptr hs_;

  /// Stores the upper layer.
  framing framing_;

  /// Stores a pointer to the owning manager for the delayed initialization.
  socket_manager* owner_ = nullptr;

  settings cfg_;
};

} // namespace caf::net::web_socket
