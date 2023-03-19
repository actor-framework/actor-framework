// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/binary_flow_bridge.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/disposable.hpp"
#include "caf/net/binary/default_trait.hpp"
#include "caf/net/binary/frame.hpp"
#include "caf/net/binary/lower_layer.hpp"
#include "caf/net/binary/upper_layer.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

namespace caf::net::lp {

/// Implements length-prefix framing for discretizing a Byte stream into
/// messages of varying size. The framing uses 4 Bytes for the length prefix,
/// but messages (including the 4 Bytes for the length prefix) are limited to a
/// maximum size of INT32_MAX. This limitation comes from the POSIX API (recv)
/// on 32-bit platforms.
class CAF_NET_EXPORT framing : public octet_stream::upper_layer,
                               public binary::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<binary::upper_layer>;

  // -- constants --------------------------------------------------------------

  static constexpr size_t hdr_size = sizeof(uint32_t);

  static constexpr size_t max_message_length = INT32_MAX - sizeof(uint32_t);

  // -- constructors, destructors, and assignment operators --------------------

  explicit framing(upper_layer_ptr up) : up_(std::move(up)) {
    // nop
  }

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<framing> make(upper_layer_ptr up);

  // -- implementation of octet_stream::upper_layer ----------------------------

  error start(octet_stream::lower_layer* down) override;

  void abort(const error& reason) override;

  ptrdiff_t consume(byte_span buffer, byte_span delta) override;

  void prepare_send() override;

  bool done_sending() override;

  // -- implementation of binary::lower_layer ----------------------------------

  bool can_send_more() const noexcept override;

  void request_messages() override;

  void suspend_reading() override;

  bool is_reading() const noexcept override;

  void write_later() override;

  void begin_message() override;

  byte_buffer& message_buffer() override;

  bool end_message() override;

  void shutdown() override;

  // -- utility functions ------------------------------------------------------

  static std::pair<size_t, byte_span> split(byte_span buffer) noexcept;

private:
  // -- member variables -------------------------------------------------------

  octet_stream::lower_layer* down_;
  upper_layer_ptr up_;
  size_t message_offset_ = 0;
};

} // namespace caf::net::lp
