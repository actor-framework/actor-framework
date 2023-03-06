// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_span.hpp"
#include "caf/detail/message_flow_bridge.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/http/header.hpp"
#include "caf/net/http/method.hpp"
#include "caf/net/http/status.hpp"
#include "caf/net/http/v1.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_oriented.hpp"
#include "caf/net/web_socket/framing.hpp"
#include "caf/net/web_socket/handshake.hpp"
#include "caf/net/web_socket/lower_layer.hpp"
#include "caf/net/web_socket/status.hpp"
#include "caf/net/web_socket/upper_layer.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"
#include "caf/string_algorithms.hpp"

#include <algorithm>

namespace caf::net::web_socket {

/// Implements the server part for the WebSocket Protocol as defined in RFC
/// 6455. Initially, the layer performs the WebSocket handshake. Once completed,
/// this layer decodes RFC 6455 frames and forwards binary and text messages to
/// `UpperLayer`.
class CAF_NET_EXPORT server : public stream_oriented::upper_layer {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<web_socket::upper_layer>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit server(upper_layer_ptr up);

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<server> make(upper_layer_ptr up);

  // -- properties -------------------------------------------------------------

  auto& upper_layer() noexcept {
    return framing_.upper_layer();
  }

  const auto& upper_layer() const noexcept {
    return framing_.upper_layer();
  }

  auto& lower_layer() noexcept {
    return framing_.lower_layer();
  }

  const auto& lower_layer() const noexcept {
    return framing_.lower_layer();
  }

  bool handshake_complete() const noexcept {
    return handshake_complete_;
  }

  // -- stream_oriented::upper_layer implementation ----------------------------

  error start(stream_oriented::lower_layer* down,
              const settings& config) override;

  void abort(const error& reason) override;

  ptrdiff_t consume(byte_span input, byte_span) override;

  void prepare_send() override;

  bool done_sending() override;

private:
  // -- HTTP request processing ------------------------------------------------

  void write_response(http::status code, std::string_view msg);

  bool handle_header(std::string_view http);

  /// Stores whether the WebSocket handshake completed successfully.
  bool handshake_complete_ = false;

  /// Stores the upper layer.
  framing framing_;

  /// Holds a copy of the settings in order to delay initialization of the upper
  /// layer until the handshake completed. We also fill this dictionary with the
  /// contents of the HTTP GET header.
  settings cfg_;
};

} // namespace caf::net::web_socket
