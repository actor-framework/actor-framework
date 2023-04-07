// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/framing.hpp"

#include "caf/logger.hpp"
#include "caf/net/http/v1.hpp"

namespace caf::net::web_socket {

// -- octet_stream::upper_layer implementation ---------------------------------

error framing::start(octet_stream::lower_layer* down) {
  std::random_device rd;
  rng_.seed(rd());
  down_ = down;
  return up_->start(this);
}

void framing::abort(const error& reason) {
  up_->abort(reason);
}

ptrdiff_t framing::consume(byte_span buffer, byte_span) {
  // Make sure we're overriding any 'exactly' setting.
  down_->configure_read(receive_policy::up_to(2048));
  // Parse header.
  detail::rfc6455::header hdr;
  auto hdr_bytes = detail::rfc6455::decode_header(buffer, hdr);
  if (hdr_bytes < 0) {
    CAF_LOG_DEBUG("decoded malformed data: hdr_bytes < 0");
    up_->abort(make_error(sec::protocol_error,
                          "negative header size on WebSocket connection"));
    return -1;
  }
  if (hdr_bytes == 0) {
    // Wait for more input.
    return 0;
  }
  // Make sure the entire frame (including header) fits into max_frame_size.
  if (hdr.payload_len >= (max_frame_size - static_cast<size_t>(hdr_bytes))) {
    CAF_LOG_DEBUG("WebSocket frame too large");
    up_->abort(make_error(sec::protocol_error, "WebSocket frame too large"));
    return -1;
  }
  // Wait for more data if necessary.
  size_t frame_size = hdr_bytes + hdr.payload_len;
  if (buffer.size() < frame_size) {
    down_->configure_read(receive_policy::exactly(frame_size));
    return 0;
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
      if (hdr.opcode == detail::rfc6455::connection_close) {
        up_->abort(make_error(sec::connection_closed));
        return -1;
      } else if (!handle(hdr.opcode, payload)) {
        return -1;
      }
    } else if (hdr.opcode != detail::rfc6455::continuation_frame) {
      CAF_LOG_DEBUG("expected a WebSocket continuation_frame");
      up_->abort(make_error(sec::protocol_error,
                            "expected a WebSocket continuation_frame"));
      return -1;
    } else if (payload_buf_.size() + payload_len > max_frame_size) {
      CAF_LOG_DEBUG("fragmented WebSocket payload exceeds maximum size");
      up_->abort(make_error(sec::protocol_error, "fragmented WebSocket payload "
                                                 "exceeds maximum size"));
      return -1;
    } else {
      if (hdr.opcode == detail::rfc6455::connection_close) {
        up_->abort(make_error(sec::connection_closed));
        return -1;
      } else {
        // End of fragmented input.
        payload_buf_.insert(payload_buf_.end(), payload.begin(), payload.end());
        if (!handle(hdr.opcode, payload_buf_)) {
          return -1;
        }
        opcode_ = nil_code;
        payload_buf_.clear();
      }
    }
  } else {
    if (opcode_ == nil_code) {
      if (hdr.opcode == detail::rfc6455::continuation_frame) {
        CAF_LOG_DEBUG("received WebSocket continuation "
                      "frame without prior opcode");
        up_->abort(make_error(sec::protocol_error,
                              "received WebSocket continuation "
                              "frame without prior opcode"));
        return -1;
      }
      opcode_ = hdr.opcode;
    } else if (payload_buf_.size() + payload_len > max_frame_size) {
      // Reject assembled payloads that exceed max_frame_size.
      CAF_LOG_DEBUG("fragmented WebSocket payload exceeds maximum size");
      up_->abort(make_error(sec::protocol_error, "fragmented WebSocket payload "
                                                 "exceeds maximum size"));
      return -1;
    }
    payload_buf_.insert(payload_buf_.end(), payload.begin(), payload.end());
  }
  return static_cast<ptrdiff_t>(frame_size);
}

void framing::prepare_send() {
  up_->prepare_send();
}

bool framing::done_sending() {
  return up_->done_sending();
}

// -- web_socket::lower_layer implementation -----------------------------------

multiplexer& framing::mpx() noexcept {
  return down_->mpx();
}

bool framing::can_send_more() const noexcept {
  return down_->can_send_more();
}

void framing::suspend_reading() {
  down_->configure_read(receive_policy::stop());
}

bool framing::is_reading() const noexcept {
  return down_->is_reading();
}

void framing::write_later() {
  down_->write_later();
}

void framing::shutdown(status code, std::string_view msg) {
  auto code_val = static_cast<uint16_t>(code);
  uint32_t mask_key = 0;
  byte_buffer payload;
  payload.reserve(msg.size() + 2);
  payload.push_back(static_cast<std::byte>((code_val & 0xFF00) >> 8));
  payload.push_back(static_cast<std::byte>(code_val & 0x00FF));
  for (auto c : msg)
    payload.push_back(static_cast<std::byte>(c));
  if (mask_outgoing_frames) {
    mask_key = static_cast<uint32_t>(rng_());
    detail::rfc6455::mask_data(mask_key, payload);
  }
  down_->begin_output();
  detail::rfc6455::assemble_frame(detail::rfc6455::connection_close, mask_key,
                                  payload, down_->output_buffer());
  down_->end_output();
  down_->shutdown();
}

void framing::request_messages() {
  if (!down_->is_reading())
    down_->configure_read(receive_policy::up_to(2048));
}

void framing::begin_binary_message() {
  // nop
}

byte_buffer& framing::binary_message_buffer() {
  return binary_buf_;
}

bool framing::end_binary_message() {
  ship_frame(binary_buf_);
  return true;
}

void framing::begin_text_message() {
  // nop
}

text_buffer& framing::text_message_buffer() {
  return text_buf_;
}

bool framing::end_text_message() {
  ship_frame(text_buf_);
  return true;
}

// -- implementation details ---------------------------------------------------

bool framing::handle(uint8_t opcode, byte_span payload) {
  // Code rfc6455::connection_close must be treated separately.
  CAF_ASSERT(opcode != detail::rfc6455::connection_close);
  switch (opcode) {
    case detail::rfc6455::text_frame: {
      std::string_view text{reinterpret_cast<const char*>(payload.data()),
                            payload.size()};
      return up_->consume_text(text) >= 0;
    }
    case detail::rfc6455::binary_frame:
      return up_->consume_binary(payload) >= 0;
    case detail::rfc6455::ping:
      ship_pong(payload);
      return true;
    case detail::rfc6455::pong:
      // nop
      return true;
    default:
      // error
      return false;
  }
}

void framing::ship_pong(byte_span payload) {
  uint32_t mask_key = 0;
  if (mask_outgoing_frames) {
    mask_key = static_cast<uint32_t>(rng_());
    detail::rfc6455::mask_data(mask_key, payload);
  }
  down_->begin_output();
  detail::rfc6455::assemble_frame(detail::rfc6455::pong, mask_key, payload,
                                  down_->output_buffer());
  down_->end_output();
}

template <class T>
void framing::ship_frame(std::vector<T>& buf) {
  uint32_t mask_key = 0;
  if (mask_outgoing_frames) {
    mask_key = static_cast<uint32_t>(rng_());
    detail::rfc6455::mask_data(mask_key, buf);
  }
  down_->begin_output();
  detail::rfc6455::assemble_frame(mask_key, buf, down_->output_buffer());
  down_->end_output();
  buf.clear();
}

} // namespace caf::net::web_socket
