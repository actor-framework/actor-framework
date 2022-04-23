// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

#include "caf/byte.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/has_after_reading.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/error.hpp"
#include "caf/net/message_flow_bridge.hpp"
#include "caf/net/message_oriented_layer_ptr.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/tag/message_oriented.hpp"
#include "caf/tag/no_auto_reading.hpp"
#include "caf/tag/stream_oriented.hpp"

namespace caf::net {

/// Length-prefixed message framing for discretizing a Byte stream into messages
/// of varying size. The framing uses 4 Bytes for the length prefix, but
/// messages (including the 4 Bytes for the length prefix) are limited to a
/// maximum size of INT32_MAX. This limitation comes from the POSIX API (recv)
/// on 32-bit platforms.
template <class UpperLayer>
class length_prefix_framing {
public:
  using input_tag = tag::stream_oriented;

  using output_tag = tag::message_oriented;

  using length_prefix_type = uint32_t;

  static constexpr size_t hdr_size = sizeof(uint32_t);

  static constexpr size_t max_message_length = INT32_MAX - sizeof(uint32_t);

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit length_prefix_framing(Ts&&... xs)
    : upper_layer_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- initialization ---------------------------------------------------------

  template <class LowerLayerPtr>
  error init(socket_manager* owner, LowerLayerPtr down, const settings& cfg) {
    if constexpr (!std::is_base_of_v<tag::no_auto_reading, UpperLayer>)
      down->configure_read(receive_policy::exactly(hdr_size));
    return upper_layer_.init(owner, this_layer_ptr(down), cfg);
  }

  // -- properties -------------------------------------------------------------

  auto& upper_layer() noexcept {
    return upper_layer_;
  }

  const auto& upper_layer() const noexcept {
    return upper_layer_;
  }

  // -- interface for the upper layer ------------------------------------------

  template <class LowerLayerPtr>
  static bool can_send_more(LowerLayerPtr down) noexcept {
    return down->can_send_more();
  }

  template <class LowerLayerPtr>
  static auto handle(LowerLayerPtr down) noexcept {
    return down->handle();
  }

  template <class LowerLayerPtr>
  static void suspend_reading(LowerLayerPtr down) {
    down->configure_read(receive_policy::stop());
  }

  template <class LowerLayerPtr>
  void begin_message(LowerLayerPtr down) {
    down->begin_output();
    auto& buf = down->output_buffer();
    message_offset_ = buf.size();
    buf.insert(buf.end(), 4, std::byte{0});
  }

  template <class LowerLayerPtr>
  byte_buffer& message_buffer(LowerLayerPtr down) {
    return down->output_buffer();
  }

  template <class LowerLayerPtr>
  [[nodiscard]] bool end_message(LowerLayerPtr down) {
    using detail::to_network_order;
    auto& buf = down->output_buffer();
    CAF_ASSERT(message_offset_ < buf.size());
    auto msg_begin = buf.begin() + message_offset_;
    auto msg_size = std::distance(msg_begin + 4, buf.end());
    if (msg_size > 0 && static_cast<size_t>(msg_size) < max_message_length) {
      auto u32_size = to_network_order(static_cast<uint32_t>(msg_size));
      memcpy(std::addressof(*msg_begin), &u32_size, 4);
      down->end_output();
      return true;
    } else {
      auto err = make_error(sec::runtime_error,
                            msg_size == 0 ? "logic error: message of size 0"
                                          : "maximum message size exceeded");
      down->abort_reason(std::move(err));
      return false;
    }
  }

  template <class LowerLayerPtr>
  bool send_close_message(LowerLayerPtr) {
    // nop; this framing layer has no close handshake
    return true;
  }

  template <class LowerLayerPtr>
  bool send_close_message(LowerLayerPtr, const error&) {
    // nop; this framing layer has no close handshake
    return true;
  }

  template <class LowerLayerPtr>
  static void abort_reason(LowerLayerPtr down, error reason) {
    return down->abort_reason(std::move(reason));
  }

  template <class LowerLayerPtr>
  static const error& abort_reason(LowerLayerPtr down) {
    return down->abort_reason();
  }

  // -- interface for the lower layer ------------------------------------------

  template <class LowerLayerPtr>
  void continue_reading(LowerLayerPtr down) {
    down->configure_read(receive_policy::exactly(hdr_size));
  }

  template <class LowerLayerPtr>
  std::enable_if_t<detail::has_after_reading_v<
    UpperLayer,
    message_oriented_layer_ptr<length_prefix_framing, LowerLayerPtr>>>
  after_reading(LowerLayerPtr down) {
    return upper_layer_.after_reading(this_layer_ptr(down));
  }

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr down) {
    return upper_layer_.prepare_send(this_layer_ptr(down));
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr down) {
    return upper_layer_.done_sending(this_layer_ptr(down));
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr down, const error& reason) {
    upper_layer_.abort(this_layer_ptr(down), reason);
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr down, byte_span input, byte_span) {
    CAF_LOG_TRACE("got" << input.size() << "bytes");
    auto this_layer = this_layer_ptr(down);
    if (input.size() < sizeof(uint32_t)) {
      auto err = make_error(sec::runtime_error,
                            "received too few bytes from underlying transport");
      down->abort_reason(std::move(err));
      return -1;
    } else if (input.size() == hdr_size) {
      auto u32_size = uint32_t{0};
      memcpy(&u32_size, input.data(), sizeof(uint32_t));
      auto msg_size = static_cast<size_t>(detail::from_network_order(u32_size));
      if (msg_size == 0) {
        // Ignore empty messages.
        CAF_LOG_DEBUG("received empty message");
        return static_cast<ptrdiff_t>(input.size());
      } else if (msg_size > max_message_length) {
        CAF_LOG_DEBUG("maximum message size exceeded");
        auto err = make_error(sec::runtime_error,
                              "maximum message size exceeded");
        down->abort_reason(std::move(err));
        return -1;
      } else {
        CAF_LOG_DEBUG("wait for payload of size" << msg_size);
        down->configure_read(receive_policy::exactly(hdr_size + msg_size));
        return 0;
      }
    } else {
      auto [msg_size, msg] = split(input);
      if (msg_size == msg.size() && msg_size + hdr_size == input.size()) {
        CAF_LOG_DEBUG("got message of size" << msg_size);
        if (upper_layer_.consume(this_layer, msg) >= 0) {
          if (!down->stopped())
            down->configure_read(receive_policy::exactly(hdr_size));
          return static_cast<ptrdiff_t>(input.size());
        } else {
          return -1;
        }
      } else {
        CAF_LOG_DEBUG("received malformed message");
        auto err = make_error(sec::runtime_error, "received malformed message");
        down->abort_reason(std::move(err));
        return -1;
      }
    }
  }

  // -- convenience functions --------------------------------------------------

  static std::pair<size_t, byte_span> split(byte_span buffer) noexcept {
    CAF_ASSERT(buffer.size() >= sizeof(uint32_t));
    auto u32_size = uint32_t{0};
    memcpy(&u32_size, buffer.data(), sizeof(uint32_t));
    auto msg_size = static_cast<size_t>(detail::from_network_order(u32_size));
    return std::make_pair(msg_size, buffer.subspan(sizeof(uint32_t)));
  }

private:
  // -- implementation details -------------------------------------------------

  template <class LowerLayerPtr>
  auto this_layer_ptr(LowerLayerPtr down) {
    return make_message_oriented_layer_ptr(this, down);
  }

  // -- member variables -------------------------------------------------------

  UpperLayer upper_layer_;
  size_t message_offset_ = 0;
};

// -- high-level factory functions -------------------------------------------

/// Runs a WebSocket server on the connected socket `fd`.
/// @param mpx The multiplexer that takes ownership of the socket.
/// @param fd A connected stream socket.
/// @param cfg Additional configuration parameters for the protocol stack.
/// @param in Inputs for writing to the socket.
/// @param out Outputs from the socket.
/// @param trait Converts between the native and the wire format.
/// @relates length_prefix_framing
template <template <class> class Transport = stream_transport, class Socket,
          class T, class Trait, class... TransportArgs>
error run_with_length_prefix_framing(multiplexer& mpx, Socket fd,
                                     const settings& cfg,
                                     async::consumer_resource<T> in,
                                     async::producer_resource<T> out,
                                     Trait trait, TransportArgs&&... args) {
  using app_t = Transport<length_prefix_framing<message_flow_bridge<T, Trait>>>;
  auto mgr = make_socket_manager<app_t>(fd, &mpx,
                                        std::forward<TransportArgs>(args)...,
                                        std::move(in), std::move(out),
                                        std::move(trait));
  return mgr->init(cfg);
}

} // namespace caf::net
