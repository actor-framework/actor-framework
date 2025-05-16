// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/web_socket/framing.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/octet_stream/lower_layer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/web_socket/lower_layer.hpp"
#include "caf/net/web_socket/status.hpp"
#include "caf/net/web_socket/upper_layer.hpp"

#include "caf/byte_span.hpp"
#include "caf/detail/rfc3629.hpp"
#include "caf/detail/rfc6455.hpp"
#include "caf/log/net.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

#include <memory>
#include <random>
#include <string_view>
#include <vector>

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

/// Checks whether the payload of a closing frame contains a valid status code
/// and an UTF-8 formatted message.
/// @returns A default constructed `error` if the payload is valid, error kind
///          otherwise.
error validate_closing_payload(const_byte_span payload) {
  if (payload.empty())
    return {};
  if (payload.size() == 1)
    return make_error(caf::sec::protocol_error,
                      "non empty closing payload must have at least two bytes");
  auto status = (std::to_integer<uint16_t>(payload[0]) << 8)
                + std::to_integer<uint16_t>(payload[1]);
  if (!detail::rfc3629::valid(payload.subspan(2)))
    return make_error(sec::protocol_error,
                      "malformed UTF-8 text message in closing payload");
  // statuses between 3000 and 4999 are allowed and application specific
  if (status >= 3000 && status < 5000)
    return {};
  // statuses between 1000 and 2999 need to be protocol defined, and status
  // codes lower then 1000 and greater or equal then 5000 are invalid.
  auto status_code = web_socket::status{0};
  if (from_integer(static_cast<uint16_t>(status), status_code)) {
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
  return make_error(sec::protocol_error,
                    "invalid status code in closing payload");
}

class framing_impl : public framing {
public:
  // -- member types -----------------------------------------------------------

  using binary_buffer = std::vector<std::byte>;

  // -- constants --------------------------------------------------------------

  /// Restricts the size of received frames (including header).
  static constexpr size_t max_frame_size = INT32_MAX;

  /// Default receive policy for a new frame.
  static constexpr auto default_receive_policy = receive_policy::up_to(2048);

  // -- constructors, destructors, and assignment operators --------------------

  explicit framing_impl(upper_layer_ptr up) : up_(std::move(up)) {
    // nop
  }

  /// When set to true, causes the layer to mask all outgoing frames with a
  /// randomly chosen masking key (cf. RFC 6455, Section 5.3). Servers may set
  /// this to false, whereas clients are required to always mask according to
  /// the standard.
  bool mask_outgoing_frames = true;

  // -- octet_stream::upper_layer implementation -------------------------------

  error start(octet_stream::lower_layer* down) override {
    std::random_device rd;
    rng_.seed(rd());
    down_ = down;
    down_->configure_read(default_receive_policy);
    return up_->start(this);
  }

  void abort(const error& reason) override {
    // Note: When closing the connection the server can send an close frame
    // without a status code. The status will be interpreted as 1005 by the
    // other side. It's illegal to set the code to 1005 or 1006 manually.
    // See RFC 6455, Section 7.1.1 and Section 7.4.
    ship_closing_message();
    up_->abort(reason);
  }

  ptrdiff_t consume(byte_span buffer, byte_span delta) override {
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

  void prepare_send() override {
    up_->prepare_send();
  }

  bool done_sending() override {
    return up_->done_sending();
  }

  // -- web_socket::lower_layer implementation ---------------------------------

  using web_socket::lower_layer::shutdown;

  socket_manager* manager() noexcept override {
    return down_->manager();
  }

  bool can_send_more() const noexcept override {
    return down_->can_send_more();
  }

  void suspend_reading() override {
    down_->configure_read(receive_policy::stop());
  }

  bool is_reading() const noexcept override {
    return down_->is_reading();
  }

  void write_later() override {
    down_->write_later();
  }

  void shutdown(status code, std::string_view msg) override {
    ship_closing_message(code, msg);
    down_->shutdown();
  }

  void request_messages() override {
    if (!down_->is_reading())
      down_->configure_read(default_receive_policy);
  }

  void begin_binary_message() override {
    // nop
  }

  byte_buffer& binary_message_buffer() override {
    return binary_buf_;
  }

  bool end_binary_message() override {
    ship_frame(binary_buf_);
    return true;
  }

  void begin_text_message() override {
    // nop
  }

  text_buffer& text_message_buffer() override {
    return text_buf_;
  }

  bool end_text_message() override {
    ship_frame(text_buf_);
    return true;
  }

private:
  // -- implementation details -------------------------------------------------

  // Validate the protocol after consuming a header.
  error validate_header(ptrdiff_t hdr_bytes) const noexcept {
    auto make_error_with_log = [](const char* message) {
      log::net::debug(message);
      return make_error(sec::protocol_error, message);
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

  // Consume the header for the currently parsing frame. Returns the number of
  // consumed bytes.
  ptrdiff_t consume_header(byte_span buffer, byte_span) {
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
    if constexpr (sizeof(size_t) < sizeof(uint64_t)) {
      if (hdr_.payload_len > std::numeric_limits<size_t>::max()) {
        abort_and_shutdown(sec::protocol_error,
                           "WebSocket frame payload exceeds maximum size");
        return -1;
      }
    }
    // Configure the buffer for the next call to consume_payload. In case of
    // text messages, we validate the UTF-8 encoding on the go, hence the use of
    // up_to.
    if (hdr_.opcode == detail::rfc6455::text_frame
        || (hdr_.opcode == detail::rfc6455::continuation_frame
            && opcode_ == detail::rfc6455::text_frame))
      down_->configure_read(receive_policy::up_to(hdr_.payload_len));
    else
      down_->configure_read(receive_policy::exactly(hdr_.payload_len));
    return hdr_bytes;
  }

  // Consume the payload for the currently parsing frame. Returns the number of
  // consumed bytes.
  ptrdiff_t consume_payload(byte_span buffer, byte_span delta) {
    // Calculate at what point of the received buffer the delta payload begins.
    auto offset = static_cast<ptrdiff_t>(buffer.size() - delta.size());
    // Unmask the arrived data.
    if (hdr_.mask_key != 0)
      detail::rfc6455::mask_data(hdr_.mask_key, buffer, offset);
    // Control frames may not me fragmented and can arrive between regular
    // message fragments.
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

  // Returns `frame_size` on success and -1 on error.
  ptrdiff_t handle(uint8_t opcode, byte_span payload, size_t frame_size) {
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

  void ship_pong(byte_span payload) {
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
  void ship_frame(std::vector<T>& buf) {
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

  // Sends closing message without a status code.
  void ship_closing_message() {
    byte_buffer payload;
    uint32_t mask_key = 0;
    // Note: Mask bit and mask key should be set even if the payload is empty.
    if (mask_outgoing_frames)
      mask_key = static_cast<uint32_t>(rng_());
    down_->begin_output();
    detail::rfc6455::assemble_frame(detail::rfc6455::connection_close_frame,
                                    mask_key, payload, down_->output_buffer());
    down_->end_output();
  }

  // Sends closing message, can be error status, or closing handshake
  void ship_closing_message(status code, std::string_view msg) {
    auto code_val = static_cast<uint16_t>(code);
    uint32_t mask_key = 0;
    byte_buffer payload;
    // GCC version 14.1.1 raising -Wfree-nonheap-object error
    CAF_PUSH_WARNINGS
    payload.reserve(msg.size() + 2);
    payload.push_back(static_cast<std::byte>((code_val >> 8) & 0x00FF));
    CAF_POP_WARNINGS
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

  // Signal abort to the upper layer and shutdown to the lower layer,
  // with closing message
  template <class... Ts>
  void abort_and_shutdown(sec reason, Ts&&... xs) {
    abort_and_shutdown(make_error(reason, std::forward<Ts>(xs)...));
  }

  void abort_and_shutdown(const error& err) {
    up_->abort(err);
    shutdown(err);
  }

  // -- member variables -------------------------------------------------------

  /// Points to the transport layer below.
  octet_stream::lower_layer* down_ = nullptr;

  /// Buffer for assembling binary frames.
  binary_buffer binary_buf_;

  /// Buffer for assembling text frames.
  text_buffer text_buf_;

  /// A 32-bit random number generator.
  std::mt19937 rng_;

  /// Header of the currently parsing frame.
  detail::rfc6455::header hdr_;

  /// Caches the opcode while decoding.
  uint8_t opcode_ = detail::rfc6455::invalid_frame;

  /// Assembles fragmented payloads.
  binary_buffer payload_buf_;

  /// Stores where to resume the UTF-8 input validation.
  size_t validation_offset_ = 0;

  /// Next layer in the processing chain.
  upper_layer_ptr up_;
};

} // namespace

// -- factories ----------------------------------------------------------------

std::unique_ptr<framing> framing::make_client(upper_layer_ptr up) {
  return std::make_unique<framing_impl>(std::move(up));
}

std::unique_ptr<framing> framing::make_server(upper_layer_ptr up) {
  // A server MUST NOT mask any frames that it sends to the client.
  // See RFC 6455, Section 5.1.
  auto res = std::make_unique<framing_impl>(std::move(up));
  res->mask_outgoing_frames = false;
  return res;
}

// -- constructors, destructors, and assignment operators --------------------

framing::~framing() {
  // nop
}

} // namespace caf::net::web_socket
