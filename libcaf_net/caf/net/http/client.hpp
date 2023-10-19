// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/response_header.hpp"
#include "caf/net/http/status.hpp"
#include "caf/net/http/upper_layer.hpp"
#include "caf/net/http/v1.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/byte_span.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/message_flow_bridge.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"
#include "caf/unordered_flat_map.hpp"

#include <algorithm>

namespace caf::net::http {

/// Implements the client part for the HTTP Protocol as defined in RFC 7231.
class CAF_NET_EXPORT client : public octet_stream::upper_layer,
                              public http::lower_layer::client {
public:
  // -- member types -----------------------------------------------------------

  enum class mode {
    read_header,
    read_payload,
    read_chunks,
  };

  using upper_layer_ptr = std::unique_ptr<http::upper_layer::client>;

  // -- constants --------------------------------------------------------------

  /// Default maximum size for incoming HTTP requests: 64KiB.
  static constexpr uint32_t default_max_request_size = 65'536;

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<client> make(upper_layer_ptr up);

  // -- constructors, destructors, and assignment operators --------------------

  explicit client(upper_layer_ptr up) : up_(std::move(up)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  auto& upper_layer() noexcept {
    return *up_;
  }

  const auto& upper_layer() const noexcept {
    return *up_;
  }

  size_t max_request_size() const noexcept {
    return max_request_size_;
  }

  void max_request_size(size_t value) noexcept {
    max_request_size_ = value;
  }

  // -- http::lower_layer::client implementation -------------------------------

  multiplexer& mpx() noexcept override;

  bool can_send_more() const noexcept override;

  bool is_reading() const noexcept override;

  void write_later() override;

  void shutdown() override;

  void request_messages() override;

  void suspend_reading() override;

  void begin_header(http::method method, std::string_view path) override;

  void add_header_field(std::string_view key, std::string_view val) override;

  bool end_header() override;

  bool send_payload(const_byte_span bytes) override;

  bool send_chunk(const_byte_span bytes) override;

  bool send_end_of_chunks() override;

  void switch_protocol(std::unique_ptr<octet_stream::upper_layer>) override;

  // -- octet_stream::upper_layer implementation -------------------------------

  error start(octet_stream::lower_layer* down) override;

  void abort(const error& reason) override;

  void prepare_send() override;

  bool done_sending() override;

  ptrdiff_t consume(byte_span input, byte_span) override;

private:
  // -- utility functions ------------------------------------------------------

  void abort_and_error(http::status status, std::string_view message) {
    up_->abort(make_error(sec::protocol_error, message));
    write_response(status, message);
  }

  void write_response(status code, std::string_view content);

  bool invoke_upper_layer(const_byte_span payload);

  bool handle_header(std::string_view http);

  /// Points to the transport layer below.
  octet_stream::lower_layer* down_;

  /// Next layer in the processing chain.
  upper_layer_ptr up_;

  /// Buffer for re-using memory.
  response_header hdr_;

  /// Stores whether we are currently waiting for the payload.
  mode mode_ = mode::read_header;

  /// Stores the expected payload size when in read_payload mode.
  size_t payload_len_ = 0;

  /// Maximum size for incoming HTTP requests.
  size_t max_request_size_ = default_max_request_size;
};

} // namespace caf::net::http
