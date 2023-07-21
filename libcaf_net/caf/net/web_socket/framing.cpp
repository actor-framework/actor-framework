// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/framing.hpp"

#include "caf/detail/rfc3629.hpp"
#include "caf/logger.hpp"
#include "caf/net/http/v1.hpp"

namespace caf::net::web_socket {

// -- static utility functions -------------------------------------------------

error framing::validate_closing_payload(const_byte_span payload) {
  if (payload.empty())
    return error{};
  if (payload.size() == 1)
    return make_error(caf::sec::protocol_error,
                      "closing status code must have two bytes");
  auto status = (std::to_integer<uint16_t>(payload[0]) << 8)
                + std::to_integer<uint16_t>(payload[1]);
  if (!detail::rfc3629::valid(payload.subspan(2)))
    return make_error(sec::protocol_error,
                      "malformed UTF-8 text message in closing payload");
  // statuses between 3000 and 4999 are allowed and application specific
  if (status >= 3000 && status < 5000)
    return error{};
  // statuses between 1000 and 2999 need to be protocol defined, and status
  // codes lower then 1000 and greater or equal then 5000 are invalid.
  auto status_code = web_socket::status{0};
  if (from_integer(status, status_code)) {
    switch (status_code) {
      case status::normal_close:
      case status::going_away:
      case status::protocol_error:
      case status::invalid_data:
      case status::inconsistent_data:
      case status::policy_violation:
      case status::message_too_big:
      case status::missing_extensions:
      case status::unexpected_condition:
        return error{};
      default:
        break;
    }
  }
  return make_error(sec::protocol_error,
                    "invalid status code in closing payload");
}

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
    abort_and_shutdown(sec::protocol_error,
                       "negative header size on WebSocket connection");
    return -1;
  }
  if (hdr_bytes == 0) {
    // Wait for more input.
    return 0;
  }
  if (detail::rfc6455::is_control_frame(hdr.opcode)) {
    // control frames can only have payload up to 125 bytes
    if (hdr.payload_len > 125) {
      CAF_LOG_DEBUG("WebSocket control frame payload exceeds allowed size");
      abort_and_shutdown(sec::protocol_error, "WebSocket control frame payload "
                                              "exceeds allowed size");
      return -1;
    }
  } else {
    // Make sure the entire frame (including header) fits into max_frame_size.
    if (hdr.payload_len >= max_frame_size - static_cast<size_t>(hdr_bytes)) {
      CAF_LOG_DEBUG("WebSocket frame too large");
      abort_and_shutdown(sec::protocol_error, "WebSocket frame too large");
      return -1;
    }
    // Reject any payload that exceeds max_frame_size. This covers assembled
    //  payloads as well by including payload_buf_.
    if (payload_buf_.size() + hdr.payload_len > max_frame_size) {
      CAF_LOG_DEBUG("fragmented WebSocket payload exceeds maximum size");
      abort_and_shutdown(sec::protocol_error, "fragmented WebSocket payload "
                                              "exceeds maximum size");
      return -1;
    }
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
  // Handle control frames first, since these may not me fragmented,
  // and can arrive between regular message fragments.
  if (detail::rfc6455::is_control_frame(hdr.opcode)
      && hdr.opcode != detail::rfc6455::continuation_frame) {
    if (!hdr.fin) {
      abort_and_shutdown(sec::protocol_error,
                         "received a fragmented WebSocket control message");
      return -1;
    }
    return handle(hdr.opcode, payload, frame_size);
  }
  if (opcode_ == nil_code
      && hdr.opcode == detail::rfc6455::continuation_frame) {
    CAF_LOG_DEBUG("received WebSocket continuation "
                  "frame without prior opcode");
    abort_and_shutdown(sec::protocol_error, "received WebSocket continuation "
                                            "frame without prior opcode");
    return -1;
  }
  if (hdr.fin) {
    if (opcode_ == nil_code) {
      // Call upper layer.
      if (hdr.opcode == detail::rfc6455::text_frame
          && !detail::rfc3629::valid(payload)) {
        abort_and_shutdown(sec::malformed_message, "invalid UTF-8 sequence");
        return -1;
      }
      return handle(hdr.opcode, payload, frame_size);
    }
    if (hdr.opcode != detail::rfc6455::continuation_frame) {
      CAF_LOG_DEBUG("expected a WebSocket continuation_frame");
      abort_and_shutdown(sec::protocol_error,
                         "expected a WebSocket continuation_frame");
      return -1;
    }
    // End of fragmented input.
    payload_buf_.insert(payload_buf_.end(), payload.begin(), payload.end());
    if (opcode_ == detail::rfc6455::text_frame && !payload_valid()) {
      abort_and_shutdown(sec::malformed_message, "invalid UTF-8 sequence");
      return -1;
    }
    auto result = handle(opcode_, payload_buf_, frame_size);
    opcode_ = nil_code;
    payload_buf_.clear();
    validation_offset_ = 0;
    return result;
  }
  // The first frame must not be a continuation frame. Any frame that is not
  // the first frame must be a continuation frame.
  if (opcode_ == nil_code) {
    opcode_ = hdr.opcode;
  } else if (hdr.opcode != detail::rfc6455::continuation_frame) {
    CAF_LOG_DEBUG("expected a continuation frame");
    abort_and_shutdown(sec::protocol_error, "expected a continuation frame");
    return -1;
  }
  payload_buf_.insert(payload_buf_.end(), payload.begin(), payload.end());
  if (opcode_ == detail::rfc6455::text_frame && !payload_valid()) {
    abort_and_shutdown(sec::malformed_message, "invalid UTF-8 sequence");
    return -1;
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
  ship_closing_message(code, msg);
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

ptrdiff_t framing::handle(uint8_t opcode, byte_span payload,
                          size_t frame_size) {
  // opcodes are checked for validity when decoding the header
  switch (opcode) {
    case detail::rfc6455::connection_close:
      if (auto error = validate_closing_payload(payload); error) {
        abort_and_shutdown(error);
        return -1;
      }
      abort_and_shutdown(sec::connection_closed);
      return -1;
    case detail::rfc6455::text_frame: {
      std::string_view text{reinterpret_cast<const char*>(payload.data()),
                            payload.size()};
      if (up_->consume_text(text) < 0)
        return -1;
      break;
    }
    case detail::rfc6455::binary_frame:
      if (up_->consume_binary(payload) < 0)
        return -1;
      break;
    case detail::rfc6455::ping:
      ship_pong(payload);
      break;
    default: //  detail::rfc6455::pong
      // nop
      break;
  }
  return static_cast<ptrdiff_t>(frame_size);
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

void framing::ship_closing_message(status code, std::string_view msg) {
  auto code_val = static_cast<uint16_t>(code);
  uint32_t mask_key = 0;
  byte_buffer payload;
  payload.reserve(msg.size() + 2);
  payload.push_back(static_cast<std::byte>((code_val & 0xFF00) >> 8));
  payload.push_back(static_cast<std::byte>(code_val & 0x00FF));
  auto* first = reinterpret_cast<const std::byte*>(msg.data());
  payload.insert(payload.end(), first, first + msg.size());
  if (mask_outgoing_frames) {
    mask_key = static_cast<uint32_t>(rng_());
    detail::rfc6455::mask_data(mask_key, payload);
  }
  down_->begin_output();
  detail::rfc6455::assemble_frame(detail::rfc6455::connection_close, mask_key,
                                  payload, down_->output_buffer());
  down_->end_output();
}

bool framing::payload_valid() noexcept {
  // validate from the index where we left off last time
  auto [index, incomplete] = detail::rfc3629::validate(
    make_span(payload_buf_).subspan(validation_offset_));
  validation_offset_ += index;
  if (validation_offset_ == payload_buf_.size())
    return true;
  // incomplete will be true if the last code point is missing continuation
  // bytes but might be valid
  return incomplete;
}

} // namespace caf::net::web_socket
