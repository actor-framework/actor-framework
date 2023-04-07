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
#include "caf/net/http/method.hpp"
#include "caf/net/http/request_header.hpp"
#include "caf/net/http/status.hpp"
#include "caf/net/http/v1.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"
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
class CAF_NET_EXPORT server : public octet_stream::upper_layer {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<web_socket::upper_layer::server>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit server(upper_layer_ptr up) : up_(std::move(up)) {
    // nop
  }

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<server> make(upper_layer_ptr up) {
    return std::make_unique<server>(std::move(up));
  }

  // -- octet_stream::upper_layer implementation -------------------------------

  error start(octet_stream::lower_layer* down) override;

  void abort(const error& reason) override;

  ptrdiff_t consume(byte_span input, byte_span) override;

  void prepare_send() override;

  bool done_sending() override;

private:
  // -- HTTP request processing ------------------------------------------------

  void write_response(http::status code, std::string_view msg);

  bool handle_header(std::string_view http);

  /// Points to the transport layer below.
  octet_stream::lower_layer* down_;

  /// We store this only to pass it to the framing layer after the handshake.
  upper_layer_ptr up_;
};

} // namespace caf::net::web_socket
