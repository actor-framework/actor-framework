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

  class CAF_NET_EXPORT upper_layer : public web_socket::upper_layer {
  public:
    virtual ~upper_layer();

    /// Initializes the upper layer.
    /// @param down A pointer to the lower layer that remains valid for the
    ///             lifetime of the upper layer.
    /// @param hdr The HTTP request header from the client handshake.
    virtual error start(lower_layer* down, const http::header& hdr) = 0;
  };

  using upper_layer_ptr = std::unique_ptr<upper_layer>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit server(upper_layer_ptr up);

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<server> make(upper_layer_ptr up);

  // -- properties -------------------------------------------------------------

  server::upper_layer& up() noexcept {
    // This cast is safe, because we know that we have initialized the framing
    // layer with a pointer to an web_socket::server::upper_layer object that
    // the framing then upcasts to web_socket::upper_layer.
    return static_cast<server::upper_layer&>(framing_.up());
  }

  const server::upper_layer& up() const noexcept {
    // See comment in the other up() overload.
    return static_cast<const server::upper_layer&>(framing_.up());
  }

  octet_stream::lower_layer& down() noexcept {
    return framing_.down();
  }

  const octet_stream::lower_layer& down() const noexcept {
    return framing_.down();
  }

  bool handshake_complete() const noexcept {
    return handshake_complete_;
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

  /// Stores whether the WebSocket handshake completed successfully.
  bool handshake_complete_ = false;

  /// Stores the upper layer.
  framing framing_;
};

} // namespace caf::net::web_socket
