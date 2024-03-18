// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/framing.hpp"

#include "caf/net/http/v1.hpp"

#include "caf/detail/rfc3629.hpp"
#include "caf/log/net.hpp"

namespace caf::net::web_socket {

namespace {

/// Checks whether the current input is valid UTF-8. Stores the last position
/// while scanning in order to avoid validating the same bytes again.
bool payload_valid(const_byte_span payload, size_t& offset) noexcept {
  // Continue from the index where we left off last time.
  auto [index, incomplete] = detail::rfc3629::validate(payload.subspan(offset));
  offset += index;
  // Incomplete will be true if the last code point is missing continuation
  // bytes but might be valid.
  return offset == payload.size() || incomplete;
}

} // namespace

// -- static utility functions -------------------------------------------------

error framing::validate_closing_payload(const_byte_span payload) {
  if (payload.empty())
    return {};
  if (payload.size() == 1)
    return error{caf::sec::protocol_error,
                 "non empty closing payload must have at least two bytes"};
  auto status = (std::to_integer<uint16_t>(payload[0]) << 8)
                + std::to_integer<uint16_t>(payload[1]);
  if (!detail::rfc3629::valid(payload.subspan(2)))
    return error{sec::protocol_error,
                 "malformed UTF-8 text message in closing payload"};
  // statuses between 3000 and 4999 are allowed and application specific
  if (status >= 3000 && status < 5000)
    return {};
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
        return {};
      default:
        break;
    }
  }
  return error{sec::protocol_error, "invalid status code in closing payload"};
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

ptrdiff_t framing::consume(byte_span buffer, byte_span delta) {
  if (!hdr_.valid()) {
    auto hdr_bytes = consume_header(buffer, delta);
    if (hdr_bytes <= 0)
      return hdr_bytes;
    if (hdr_.payload_len == 0
        && consume_payload(buffer.first(0), delta.first(0)) < 0)
      return -1;
    return hdr_bytes;
  }
  return consume_payload(buffer, delta);
}

void framing::prepare_send() {
  up_->prepare_send();
}

bool framing::done_sending() {
  return up_->done_sending();
}

// -- web_socket::lower_layer implementation -----------------------------------

socket_manager* framing::manager() noexcept {
  return down_->manager();
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
    down_->configure_read(default_receive_policy);
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

error framing::validate_header(ptrdiff_t hdr_bytes) const noexcept {
  auto make_error_with_log = [](const char* message) {
    log::net::debug(message);
    return error{sec::protocol_error, message};
  };
  if (detail::rfc6455::is_control_frame(hdr_.opcode)) {
    // Control frames can have a payload up to 125 bytes and can't be
    // fragmented.
    if (hdr_.payload_len > 125)
      return make_error_with_log(
        "WebSocket control frame payload exceeds allowed size");
    if (!hdr_.fin)
      return make_error_with_log(
        "Received a fragmented WebSocket control message");
  } else {
    // The opcode is either continuation, text of binary frame.
    // Make sure the entire frame (including header) fits into max_frame_size.
    if (hdr_.payload_len >= max_frame_size - static_cast<size_t>(hdr_bytes))
      return make_error_with_log("WebSocket frame too large");
    // Reject any message whose assembled payload size exceeds max_frame_size.
    if (payload_buf_.size() + hdr_.payload_len > max_frame_size)
      return make_error_with_log(
        "Fragmented WebSocket payload exceeds maximum size");
    if (hdr_.opcode != detail::rfc6455::continuation_frame
        && opcode_ != detail::rfc6455::invalid_frame)
      return make_error_with_log("Expected a WebSocket continuation_frame");
    if (hdr_.opcode == detail::rfc6455::continuation_frame
        && opcode_ == detail::rfc6455::invalid_frame)
      return make_error_with_log(
        "Received WebSocket continuation frame without prior opcode");
  }
  return none;
}

ptrdiff_t framing::consume_header(byte_span buffer, byte_span) {
  // Parse header.
  auto hdr_bytes = detail::rfc6455::decode_header(buffer, hdr_);
  if (hdr_bytes < 0) {
    log::net::debug("decoded malformed data: hdr_bytes < 0");
    abort_and_shutdown(sec::protocol_error,
                       "negative header size on WebSocket connection");
    return -1;
  }
  // Wait for more input.
  if (hdr_bytes == 0)
    return 0;
  if (auto err = validate_header(hdr_bytes); err) {
    abort_and_shutdown(err);
    return -1;
  }
  // Configure the buffer for the next call to consume_payload. In case of text
  // messages, we validate the UTF-8 encoding on the go, hence the use of up_to.
  if (hdr_.opcode == detail::rfc6455::text_frame
      || (hdr_.opcode == detail::rfc6455::continuation_frame
          && opcode_ == detail::rfc6455::text_frame))
    down_->configure_read(receive_policy::up_to(hdr_.payload_len));
  else
    down_->configure_read(receive_policy::exactly(hdr_.payload_len));
  return hdr_bytes;
}

ptrdiff_t framing::consume_payload(byte_span buffer, byte_span delta) {
  // Calculate at what point of the received buffer the delta payload begins.
  auto offset = static_cast<ptrdiff_t>(buffer.size() - delta.size());
  // Unmask the arrived data.
  if (hdr_.mask_key != 0)
    detail::rfc6455::mask_data(hdr_.mask_key, buffer, offset);
  // Control frames may not me fragmented and can arrive between regular message
  // fragments.
  if (detail::rfc6455::is_control_frame(hdr_.opcode))
    return handle(hdr_.opcode, buffer, hdr_.payload_len);
  // Handle the fragmentation logic of text and binary messages.
  if (hdr_.opcode == detail::rfc6455::text_frame
      || opcode_ == detail::rfc6455::text_frame) {
    // For text messages we validate the UTF-8 encoding on the go. Only text
    // messages can arrive with incomplete payload.
    if (hdr_.opcode == detail::rfc6455::text_frame && hdr_.fin) {
      if (!payload_valid(buffer, validation_offset_)) {
        abort_and_shutdown(sec::malformed_message, "Invalid UTF-8 sequence");
        return -1;
      }
    } else {
      payload_buf_.insert(payload_buf_.end(), buffer.begin() + offset,
                          buffer.end());
      if (!payload_valid(payload_buf_, validation_offset_)) {
        abort_and_shutdown(sec::malformed_message, "Invalid UTF-8 sequence");
        return -1;
      }
    }
    // Wait for more data if necessary.
    if (buffer.size() < hdr_.payload_len)
      return 0;
  } else if ((hdr_.opcode == detail::rfc6455::binary_frame && !hdr_.fin)
             || opcode_ == detail::rfc6455::binary_frame) {
    payload_buf_.insert(payload_buf_.end(), buffer.begin(), buffer.end());
  }
  // Handle the completed frame.
  if (hdr_.fin) {
    if (opcode_ == detail::rfc6455::invalid_frame) {
      if (hdr_.opcode == detail::rfc6455::text_frame
          && validation_offset_ != buffer.size()) {
        abort_and_shutdown(sec::malformed_message, "Invalid UTF-8 sequence");
        return -1;
      }
      // Call upper layer.
      validation_offset_ = 0;
      return handle(hdr_.opcode, buffer, hdr_.payload_len);
    }
    // End of fragmented input.
    if (opcode_ == detail::rfc6455::text_frame
        && validation_offset_ != payload_buf_.size()) {
      abort_and_shutdown(sec::malformed_message, "Invalid UTF-8 sequence");
      return -1;
    }
    auto result = handle(opcode_, payload_buf_, hdr_.payload_len);
    opcode_ = detail::rfc6455::invalid_frame;
    payload_buf_.clear();
    validation_offset_ = 0;
    return result;
  }
  if (opcode_ == detail::rfc6455::invalid_frame)
    opcode_ = hdr_.opcode;
  // Clean up the state since we finished processing this frame
  down_->configure_read(default_receive_policy);
  hdr_.opcode = detail::rfc6455::invalid_frame;
  return static_cast<ptrdiff_t>(hdr_.payload_len);
}

ptrdiff_t framing::handle(uint8_t opcode, byte_span payload,
                          size_t frame_size) {
  // opcodes are checked for validity when decoding the header
  switch (opcode) {
    case detail::rfc6455::connection_close_frame:
      if (auto err = validate_closing_payload(payload); err) {
        abort_and_shutdown(err);
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
    case detail::rfc6455::ping_frame:
      ship_pong(payload);
      break;
    default: //  detail::rfc6455::pong
      // nop
      break;
  }
  // Clean up the state since we finished processing this frame
  down_->configure_read(default_receive_policy);
  hdr_.opcode = detail::rfc6455::invalid_frame;
  return static_cast<ptrdiff_t>(frame_size);
}

void framing::ship_pong(byte_span payload) {
  uint32_t mask_key = 0;
  if (mask_outgoing_frames) {
    mask_key = static_cast<uint32_t>(rng_());
    detail::rfc6455::mask_data(mask_key, payload);
  }
  down_->begin_output();
  detail::rfc6455::assemble_frame(detail::rfc6455::pong_frame, mask_key,
                                  payload, down_->output_buffer());
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
  detail::rfc6455::assemble_frame(detail::rfc6455::connection_close_frame,
                                  mask_key, payload, down_->output_buffer());
  down_->end_output();
}

} // namespace caf::net::web_socket
