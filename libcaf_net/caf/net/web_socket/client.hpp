// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <algorithm>

#include "caf/byte_span.hpp"
#include "caf/detail/encode_base64.hpp"
#include "caf/error.hpp"
#include "caf/hash/sha1.hpp"
#include "caf/logger.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/web_socket/handshake.hpp"
#include "caf/net/web_socket/framing.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"
#include "caf/tag/mixed_message_oriented.hpp"
#include "caf/tag/stream_oriented.hpp"

namespace caf::net::web_socket {

/// Implements the client part for the WebSocket Protocol as defined in RFC
/// 6455. Initially, the layer performs the WebSocket handshake. Once completed,
/// this layer decodes RFC 6455 frames and forwards binary and text messages to
/// `UpperLayer`.
template <class UpperLayer>
class client {
public:
  // -- member types -----------------------------------------------------------

  using input_tag = tag::stream_oriented;

  using output_tag = tag::mixed_message_oriented;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit client(handshake hs, Ts&&... xs)
    : upper_layer_(std::forward<Ts>(xs)...) {
    handshake_.reset(new handshake(std::move(hs)));
  }

  // -- initialization ---------------------------------------------------------

  template <class LowerLayerPtr>
  error init(socket_manager* owner, LowerLayerPtr down, const settings& cfg) {
    CAF_ASSERT(handshake_ != nullptr);
    owner_ = owner;
    if (!handshake_->has_valid_key())
      return make_error(sec::runtime_error, "handshake data lacks a valid key");
    if (!handshake_->has_mandatory_fields())
      return make_error(sec::runtime_error,
                        "handshake data lacks mandatory fields");
    cfg_ = cfg;
    down->begin_output();
    handshake_->write_http_1_request(down->output_buffer());
    down->end_output();
    down->configure_read(receive_policy::up_to(handshake::max_http_size));
    return none;
  }

  // -- properties -------------------------------------------------------------

  auto& upper_layer() noexcept {
    return upper_layer_.upper_layer();
  }

  const auto& upper_layer() const noexcept {
    return upper_layer_.upper_layer();
  }

  // -- role: upper layer ------------------------------------------------------

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr down) {
    return handshake_complete() ? upper_layer_.prepare_send(down) : true;
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr down) {
    return handshake_complete() ? upper_layer_.done_sending(down) : true;
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr down, const error& reason) {
    if (handshake_complete())
      upper_layer_.abort(down, reason);
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr down, byte_span input, byte_span delta) {
    CAF_LOG_TRACE(CAF_ARG2("socket", down->handle().id)
                  << CAF_ARG2("bytes", input.size()));
    // Short circuit to the framing layer after the handshake completed.
    if (handshake_complete())
      return upper_layer_.consume(down, input, delta);
    // Check whether received a HTTP header or else wait for more data or abort
    // when exceeding the maximum size.
    auto [hdr, remainder] = handshake::split_http_1_header(input);
    if (hdr.empty()) {
      if (input.size() >= handshake::max_http_size) {
        down->begin_output();
        handshake::write_http_1_header_too_large(down->output_buffer());
        down->end_output();
        auto err = make_error(pec::too_many_characters,
                              "exceeded maximum header size");
        down->abort_reason(std::move(err));
        return -1;
      } else {
        return 0;
      }
    } else if (!handle_header(down, hdr)) {
      return -1;
    } else if (remainder.empty()) {
      CAF_ASSERT(hdr.size() == input.size());
      return hdr.size();
    } else {
      CAF_LOG_DEBUG(CAF_ARG2("socket", down->handle().id)
                    << CAF_ARG2("remainder", remainder.size()));
      if (auto sub_result = upper_layer_.consume(down, remainder, remainder);
          sub_result >= 0) {
        return hdr.size() + sub_result;
      } else {
        return sub_result;
      }
    }
  }

  bool handshake_complete() const noexcept {
    return handshake_ == nullptr;
  }

private:
  // -- HTTP response processing -----------------------------------------------

  template <class LowerLayerPtr>
  bool handle_header(LowerLayerPtr down, string_view http) {
    CAF_ASSERT(handshake_ != nullptr);
    auto http_ok = handshake_->is_valid_http_1_response(http);
    handshake_.reset();
    if (http_ok) {
      if (auto err = upper_layer_.init(owner_, down, cfg_)) {
        CAF_LOG_DEBUG("failed to initialize WebSocket framing layer");
        down->abort_reason(std::move(err));
        return false;
      } else {
        CAF_LOG_DEBUG("completed WebSocket handshake");
        return true;
      }
    } else {
      CAF_LOG_DEBUG("received invalid WebSocket handshake");
      down->abort_reason(make_error(
        sec::runtime_error,
        "received invalid WebSocket handshake response from server"));
      return false;
    }
  }

  // -- member variables -------------------------------------------------------

  /// Stores the WebSocket handshake data until the handshake completed.
  std::unique_ptr<handshake> handshake_;

  /// Stores the upper layer.
  framing<UpperLayer> upper_layer_;

  /// Stores a pointer to the owning manager for the delayed initialization.
  socket_manager* owner_ = nullptr;

  settings cfg_;
};

} // namespace caf::net::web_socket
