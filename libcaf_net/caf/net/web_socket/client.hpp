// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_span.hpp"
#include "caf/detail/base64.hpp"
#include "caf/detail/message_flow_bridge.hpp"
#include "caf/error.hpp"
#include "caf/hash/sha1.hpp"
#include "caf/logger.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/http/v1.hpp"
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
class CAF_NET_EXPORT client : public stream_oriented::upper_layer {
public:
  // -- member types -----------------------------------------------------------

  class CAF_NET_EXPORT upper_layer : public web_socket::upper_layer {
  public:
    virtual ~upper_layer();

    /// Initializes the upper layer.
    /// @param down A pointer to the lower layer that remains valid for the
    ///             lifetime of the upper layer.
    virtual error start(lower_layer* down) = 0;
  };

  using handshake_ptr = std::unique_ptr<handshake>;

  using upper_layer_ptr = std::unique_ptr<upper_layer>;

  // -- constructors, destructors, and assignment operators --------------------

  client(handshake_ptr hs, upper_layer_ptr up);

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<client> make(handshake_ptr hs, upper_layer_ptr up);

  static std::unique_ptr<client> make(handshake&& hs, upper_layer_ptr up) {
    return make(std::make_unique<handshake>(std::move(hs)), std::move(up));
  }

  // -- properties -------------------------------------------------------------

  client::upper_layer& up() noexcept {
    // This cast is safe, because we know that we have initialized the framing
    // layer with a pointer to an web_socket::client::upper_layer object that
    // the framing then upcasts to web_socket::upper_layer.
    return static_cast<client::upper_layer&>(framing_.up());
  }

  const client::upper_layer& up() const noexcept {
    // See comment in the other up() overload.
    return static_cast<const client::upper_layer&>(framing_.up());
  }

  stream_oriented::lower_layer& down() noexcept {
    return framing_.down();
  }

  const stream_oriented::lower_layer& down() const noexcept {
    return framing_.down();
  }

  bool handshake_completed() const noexcept {
    return hs_ == nullptr;
  }

  // -- implementation of stream_oriented::upper_layer -------------------------

  error start(stream_oriented::lower_layer* down) override;

  void abort(const error& reason) override;

  ptrdiff_t consume(byte_span buffer, byte_span delta) override;

  void prepare_send() override;

  bool done_sending() override;

private:
  // -- HTTP response processing -----------------------------------------------

  bool handle_header(std::string_view http);

  // -- member variables -------------------------------------------------------

  /// Stores the WebSocket handshake data until the handshake completed.
  handshake_ptr hs_;

  /// Stores the upper layer.
  framing framing_;

  settings cfg_;
};

} // namespace caf::net::web_socket
