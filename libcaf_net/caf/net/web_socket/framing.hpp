// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/rfc6455.hpp"
#include "caf/net/mixed_message_oriented_layer_ptr.hpp"
#include "caf/net/web_socket/status.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"
#include "caf/tag/mixed_message_oriented.hpp"
#include "caf/tag/stream_oriented.hpp"

#include <random>
#include <string_view>
#include <vector>

namespace caf::net::web_socket {

/// Implements the WebSocket framing protocol as defined in RFC-6455.
template <class UpperLayer>
class framing {
public:
  // -- member types -----------------------------------------------------------

  using binary_buffer = std::vector<std::byte>;

  using text_buffer = std::vector<char>;

  using input_tag = tag::stream_oriented;

  using output_tag = tag::mixed_message_oriented;

  // -- constants --------------------------------------------------------------

  /// Restricts the size of received frames (including header).
  static constexpr size_t max_frame_size = INT32_MAX;

  /// Stored as currently active opcode to mean "no opcode received yet".
  static constexpr size_t nil_code = 0xFF;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit framing(Ts&&... xs) : upper_layer_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- initialization ---------------------------------------------------------

  template <class LowerLayerPtr>
  error init(socket_manager* owner, LowerLayerPtr down, const settings& cfg) {
    std::random_device rd;
    rng_.seed(rd());
    return upper_layer_.init(owner, this_layer_ptr(down), cfg);
  }

  // -- properties -------------------------------------------------------------

  auto& upper_layer() noexcept {
    return upper_layer_;
  }

  const auto& upper_layer() const noexcept {
    return upper_layer_;
  }

  /// When set to true, causes the layer to mask all outgoing frames with a
  /// randomly chosen masking key (cf. RFC 6455, Section 5.3). Servers may set
  /// this to false, whereas clients are required to always mask according to
  /// the standard.
  bool mask_outgoing_frames = true;

  // -- interface for mixed_message_oriented_layer_ptr -------------------------

  template <class LowerLayerPtr>
  static bool can_send_more(LowerLayerPtr parent) noexcept {
    return parent->can_send_more();
  }

  template <class LowerLayerPtr>
  static auto handle(LowerLayerPtr parent) noexcept {
    return parent->handle();
  }

  template <class LowerLayerPtr>
  static void suspend_reading(LowerLayerPtr down) {
    down->configure_read(receive_policy::stop());
  }

  template <class LowerLayerPtr>
  static constexpr void begin_binary_message(LowerLayerPtr) {
    // nop
  }

  template <class LowerLayerPtr>
  binary_buffer& binary_message_buffer(LowerLayerPtr) {
    return binary_buf_;
  }

  template <class LowerLayerPtr>
  bool end_binary_message(LowerLayerPtr down) {
    ship_frame(down, binary_buf_);
    return true;
  }

  template <class LowerLayerPtr>
  static constexpr void begin_text_message(LowerLayerPtr) {
    // nop
  }

  template <class LowerLayerPtr>
  text_buffer& text_message_buffer(LowerLayerPtr) {
    return text_buf_;
  }

  template <class LowerLayerPtr>
  bool end_text_message(LowerLayerPtr down) {
    ship_frame(down, text_buf_);
    return true;
  }

  template <class LowerLayerPtr>
  bool send_close_message(LowerLayerPtr down) {
    ship_close(down);
    return true;
  }

  template <class LowerLayerPtr>
  bool
  send_close_message(LowerLayerPtr down, status code, std::string_view desc) {
    ship_close(down, static_cast<uint16_t>(code), desc);
    return true;
  }

  template <class LowerLayerPtr>
  bool send_close_message(LowerLayerPtr down, const error& reason) {
    ship_close(down, static_cast<uint16_t>(status::unexpected_condition),
               to_string(reason));
    return true;
  }

  template <class LowerLayerPtr>
  static void abort_reason(LowerLayerPtr parent, error reason) {
    return parent->abort_reason(std::move(reason));
  }

  template <class LowerLayerPtr>
  static const error& abort_reason(LowerLayerPtr parent) {
    return parent->abort_reason();
  }

  // -- interface for the lower layer ------------------------------------------

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr down) {
    return upper_layer_.prepare_send(this_layer_ptr(down));
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr down) {
    return upper_layer_.done_sending(this_layer_ptr(down));
  }

  template <class LowerLayerPtr>
  static void continue_reading(LowerLayerPtr down) {
    down->configure_read(receive_policy::up_to(2048));
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr down, const error& reason) {
    upper_layer_.abort(down, reason);
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr down, byte_span input, byte_span) {
    auto buffer = input;
    ptrdiff_t consumed = 0;
    // Parse all frames in the current input.
    for (;;) {
      // Parse header.
      detail::rfc6455::header hdr;
      auto hdr_bytes = detail::rfc6455::decode_header(buffer, hdr);
      if (hdr_bytes < 0) {
        down->abort_reason(
          make_error(sec::runtime_error, "invalid WebSocket frame header"));
        return -1;
      }
      if (hdr_bytes == 0) {
        // Wait for more input.
        down->configure_read(receive_policy::up_to(2048));
        return consumed;
      }
      // Make sure the entire frame (including header) fits into max_frame_size.
      if (hdr.payload_len
          >= (max_frame_size - static_cast<size_t>(hdr_bytes))) {
        auto err = make_error(sec::runtime_error, "WebSocket frame too large");
        down->abort_reason(std::move(err));
        return -1;
      }
      // Wait for more data if necessary.
      size_t frame_size = hdr_bytes + hdr.payload_len;
      if (buffer.size() < frame_size) {
        // Ask for more data unless the upper layer called suspend_reading.
        if (!down->stopped())
          down->configure_read(receive_policy::up_to(2048));
        down->configure_read(receive_policy::exactly(frame_size));
        return consumed;
      }
      // Decode frame.
      auto payload_len = static_cast<size_t>(hdr.payload_len);
      auto payload = buffer.subspan(hdr_bytes, payload_len);
      if (hdr.mask_key != 0) {
        detail::rfc6455::mask_data(hdr.mask_key, payload);
      }
      if (hdr.fin) {
        if (opcode_ == nil_code) {
          // Call upper layer.
          if (!handle(down, hdr.opcode, payload))
            return -1;
        } else if (hdr.opcode != detail::rfc6455::continuation_frame) {
          // Reject continuation frames without prior opcode.
          auto err = make_error(sec::runtime_error,
                                "invalid WebSocket frame "
                                "(expected a continuation frame)");
          down->abort_reason(std::move(err));
          return -1;
        } else if (payload_buf_.size() + payload_len > max_frame_size) {
          // Reject assembled payloads that exceed max_frame_size.
          auto err = make_error(sec::runtime_error,
                                "fragmented payload exceeds maximum size");
          down->abort_reason(std::move(err));
          return -1;
        } else {
          // End of fragmented input.
          payload_buf_.insert(payload_buf_.end(), payload.begin(),
                              payload.end());
          if (!handle(down, hdr.opcode, payload_buf_))
            return -1;
          opcode_ = nil_code;
          payload_buf_.clear();
        }
      } else {
        if (opcode_ == nil_code) {
          if (hdr.opcode == detail::rfc6455::continuation_frame) {
            // Reject continuation frames without prior opcode.
            auto err = make_error(sec::runtime_error,
                                  "invalid WebSocket continuation frame "
                                  "(no prior opcode)");
            down->abort_reason(std::move(err));
            return -1;
          }
          opcode_ = hdr.opcode;
        } else if (payload_buf_.size() + payload_len > max_frame_size) {
          // Reject assembled payloads that exceed max_frame_size.
          auto err = make_error(sec::runtime_error,
                                "fragmented payload exceeds maximum size");
          down->abort_reason(std::move(err));
          return -1;
        }
        payload_buf_.insert(payload_buf_.end(), payload.begin(), payload.end());
      }
      // Advance to next frame in the input.
      buffer = buffer.subspan(frame_size);
      if (buffer.empty()) {
        // Ask for more data unless the upper layer called suspend_reading.
        if (!down->stopped())
          down->configure_read(receive_policy::up_to(2048));
        return consumed + static_cast<ptrdiff_t>(frame_size);
      }
      consumed += static_cast<ptrdiff_t>(frame_size);
    }
  }

private:
  // -- implementation details -------------------------------------------------

  template <class LowerLayerPtr>
  bool handle(LowerLayerPtr down, uint8_t opcode, byte_span payload) {
    switch (opcode) {
      case detail::rfc6455::text_frame: {
        std::string_view text{reinterpret_cast<const char*>(payload.data()),
                              payload.size()};
        return upper_layer_.consume_text(this_layer_ptr(down), text) >= 0;
      }
      case detail::rfc6455::binary_frame:
        return upper_layer_.consume_binary(this_layer_ptr(down), payload) >= 0;
      case detail::rfc6455::connection_close:
        down->abort_reason(sec::connection_closed);
        return false;
      case detail::rfc6455::ping:
        ship_pong(down, payload);
        return true;
      case detail::rfc6455::pong:
        // nop
        return true;
      default:
        // nop
        return false;
    }
  }

  template <class LowerLayerPtr>
  void ship_pong(LowerLayerPtr down, byte_span payload) {
    uint32_t mask_key = 0;
    if (mask_outgoing_frames) {
      mask_key = static_cast<uint32_t>(rng_());
      detail::rfc6455::mask_data(mask_key, payload);
    }
    down->begin_output();
    detail::rfc6455::assemble_frame(detail::rfc6455::pong, mask_key, payload,
                                    down->output_buffer());
    down->end_output();
  }

  template <class LowerLayerPtr>
  void ship_close(LowerLayerPtr down, uint16_t code, std::string_view msg) {
    uint32_t mask_key = 0;
    byte_buffer payload;
    payload.reserve(msg.size() + 2);
    payload.push_back(static_cast<std::byte>((code & 0xFF00) >> 8));
    payload.push_back(static_cast<std::byte>(code & 0x00FF));
    for (auto c : msg)
      payload.push_back(static_cast<std::byte>(c));
    if (mask_outgoing_frames) {
      mask_key = static_cast<uint32_t>(rng_());
      detail::rfc6455::mask_data(mask_key, payload);
    }
    down->begin_output();
    detail::rfc6455::assemble_frame(detail::rfc6455::connection_close, mask_key,
                                    payload, down->output_buffer());
    down->end_output();
  }

  template <class LowerLayerPtr>
  void ship_close(LowerLayerPtr down) {
    uint32_t mask_key = 0;
    std::byte payload[] = {// Error code 1000: normal close.
                           std::byte{0x03}, std::byte{0xE8},
                           // "EOF" string as goodbye message.
                           std::byte{'E'}, std::byte{'O'}, std::byte{'F'}};
    if (mask_outgoing_frames) {
      mask_key = static_cast<uint32_t>(rng_());
      detail::rfc6455::mask_data(mask_key, payload);
    }
    down->begin_output();
    detail::rfc6455::assemble_frame(detail::rfc6455::connection_close, mask_key,
                                    payload, down->output_buffer());
    down->end_output();
  }

  template <class LowerLayerPtr, class T>
  void ship_frame(LowerLayerPtr down, std::vector<T>& buf) {
    uint32_t mask_key = 0;
    if (mask_outgoing_frames) {
      mask_key = static_cast<uint32_t>(rng_());
      detail::rfc6455::mask_data(mask_key, buf);
    }
    down->begin_output();
    detail::rfc6455::assemble_frame(mask_key, buf, down->output_buffer());
    down->end_output();
    buf.clear();
  }

  template <class LowerLayerPtr>
  auto this_layer_ptr(LowerLayerPtr down) {
    return make_mixed_message_oriented_layer_ptr(this, down);
  }

  // -- member variables -------------------------------------------------------

  // Buffer for assembling binary frames.
  binary_buffer binary_buf_;

  // Buffer for assembling text frames.
  text_buffer text_buf_;

  // 32-bit random number generator.
  std::mt19937 rng_;

  uint8_t opcode_ = nil_code;

  // Assembles fragmented payloads.
  binary_buffer payload_buf_;

  // Next layer in the processing chain.
  UpperLayer upper_layer_;
};

} // namespace caf::net::web_socket
