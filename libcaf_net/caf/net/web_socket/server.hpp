// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/web_socket/upper_layer.hpp"

#include "caf/detail/net_export.hpp"

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

  ~server() override;

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<server> make(upper_layer_ptr up);
};

} // namespace caf::net::web_socket
