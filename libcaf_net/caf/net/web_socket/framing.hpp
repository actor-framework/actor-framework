// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/octet_stream/lower_layer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/web_socket/lower_layer.hpp"
#include "caf/net/web_socket/status.hpp"
#include "caf/net/web_socket/upper_layer.hpp"

#include "caf/byte.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/rfc6455.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"

#include <memory>
#include <random>
#include <string_view>
#include <vector>

namespace caf::net::web_socket {

/// Implements the WebSocket framing protocol as defined in RFC-6455.
class CAF_NET_EXPORT framing : public octet_stream::upper_layer,
                               public web_socket::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using binary_buffer = std::vector<std::byte>;

  using upper_layer_ptr = std::unique_ptr<web_socket::upper_layer>;

  // -- constants --------------------------------------------------------------

  /// Restricts the size of received frames (including header).
  static constexpr size_t max_frame_size = INT32_MAX;

  /// Default receive policy for a new frame.
  static constexpr auto default_receive_policy = receive_policy::up_to(2048);

  // -- static utility functions -----------------------------------------------

  /// Checks whether the payload of a closing frame contains a valid status
  /// code and an UTF-8 formatted message.
  /// @returns A default constructed `error` if the payload is valid, error kind
  ///          otherwise.
  static error validate_closing_payload(const_byte_span payload);

  // -- constructors, destructors, and assignment operators --------------------

  /// Creates a new framing protocol for client mode.
  static std::unique_ptr<framing> make_client(upper_layer_ptr up) {
    return std::unique_ptr<framing>{new framing(std::move(up))};
  }

  /// Creates a new framing protocol for server mode.
  static std::unique_ptr<framing> make_server(upper_layer_ptr up) {
    // > A server MUST NOT mask any frames that it sends to the client.
    // See RFC 6455, Section 5.1.
    auto res = std::unique_ptr<framing>{new framing(std::move(up))};
    res->mask_outgoing_frames = false;
    return res;
  }

  // -- properties -------------------------------------------------------------

  auto& up() noexcept {
    return *up_;
  }

  const auto& up() const noexcept {
    return *up_;
  }

  auto& down() noexcept {
    return *down_;
  }

  const auto& down() const noexcept {
    return *down_;
  }

  /// When set to true, causes the layer to mask all outgoing frames with a
  /// randomly chosen masking key (cf. RFC 6455, Section 5.3). Servers may set
  /// this to false, whereas clients are required to always mask according to
  /// the standard.
  bool mask_outgoing_frames = true;

  // -- octet_stream::upper_layer implementation -------------------------------

  error start(octet_stream::lower_layer* down) override;

  void abort(const error& reason) override;

  ptrdiff_t consume(byte_span input, byte_span) override;

  void prepare_send() override;

  bool done_sending() override;

  // -- web_socket::lower_layer implementation ---------------------------------

  using web_socket::lower_layer::shutdown;

  multiplexer& mpx() noexcept override;

  bool can_send_more() const noexcept override;

  void suspend_reading() override;

  bool is_reading() const noexcept override;

  void write_later() override;

  void shutdown(status code, std::string_view desc) override;

  void request_messages() override;

  void begin_binary_message() override;

  byte_buffer& binary_message_buffer() override;

  bool end_binary_message() override;

  void begin_text_message() override;

  text_buffer& text_message_buffer() override;

  bool end_text_message() override;

private:
  // -- implementation details -------------------------------------------------

  explicit framing(upper_layer_ptr up) : up_(std::move(up)) {
    // nop
  }

  // Validate the protocol after consuming a header.
  error validate_header(ptrdiff_t hdr_bytes) const noexcept;

  // Consume the header for the currently parsing frame. Returns the number of
  // sonsumed bytes.
  ptrdiff_t consume_header(byte_span input, byte_span);

  // Consume the payload for the currently parsing frame. Returns the number of
  // consumed bytes.
  ptrdiff_t consume_payload(byte_span buffer, byte_span delta);

  // Returns `frame_size` on success and -1 on error.
  ptrdiff_t handle(uint8_t opcode, byte_span payload, size_t frame_size);

  void ship_pong(byte_span payload);

  template <class T>
  void ship_frame(std::vector<T>& buf);

  // Sends closing message, can be error status, or closing handshake
  void ship_closing_message(status code, std::string_view desc);

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

} // namespace caf::net::web_socket
