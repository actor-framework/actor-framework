// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <cstring>
#include <memory>

#include "caf/byte.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/error.hpp"
#include "caf/net/message_oriented_layer_ptr.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/tag/message_oriented.hpp"
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

  static constexpr size_t max_message_length = INT32_MAX - sizeof(uint32_t);

  static constexpr uint32_t default_receive_size = 4 * 1024; // 4kb.

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit length_prefix_framing(Ts&&... xs)
    : upper_layer_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- initialization ---------------------------------------------------------

  template <class LowerLayerPtr>
  error init(socket_manager* owner, LowerLayerPtr down, const settings& cfg) {
    down->configure_read(
      receive_policy::between(sizeof(uint32_t), default_receive_size));
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
  void begin_message(LowerLayerPtr down) {
    down->begin_output();
    auto& buf = down->output_buffer();
    message_offset_ = buf.size();
    buf.insert(buf.end(), 4, byte{0});
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
  static void abort_reason(LowerLayerPtr down, error reason) {
    return down->abort_reason(std::move(reason));
  }

  template <class LowerLayerPtr>
  static const error& abort_reason(LowerLayerPtr down) {
    return down->abort_reason();
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
  void abort(LowerLayerPtr down, const error& reason) {
    upper_layer_.abort(this_layer_ptr(down), reason);
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr down, byte_span input, byte_span) {
    auto buffer = input;
    auto consumed = ptrdiff_t{0};
    auto this_layer = this_layer_ptr(down);
    for (;;) {
      if (input.size() < sizeof(uint32_t)) {
        return consumed;
      } else {
        auto [msg_size, sub_buffer] = split(input);
        if (msg_size == 0) {
          consumed += static_cast<ptrdiff_t>(sizeof(uint32_t));
          input = sub_buffer;
        } else if (msg_size > max_message_length) {
          auto err = make_error(sec::runtime_error,
                                "maximum message size exceeded");
          down->abort_reason(std::move(err));
          return -1;
        } else if (msg_size > sub_buffer.size()) {
          if (msg_size + sizeof(uint32_t) > receive_buf_upper_bound_) {
            auto min_read_size = static_cast<uint32_t>(sizeof(uint32_t));
            receive_buf_upper_bound_
              = static_cast<uint32_t>(msg_size + sizeof(uint32_t));
            down->configure_read(
              receive_policy::between(min_read_size, receive_buf_upper_bound_));
          }
          return consumed;
        } else {
          auto msg = sub_buffer.subspan(0, msg_size);
          if (auto res = upper_layer_.consume(this_layer, msg); res >= 0) {
            consumed += static_cast<ptrdiff_t>(msg.size()) + sizeof(uint32_t);
            input = sub_buffer.subspan(msg_size);
          } else {
            return -1;
          }
        }
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
  uint32_t receive_buf_upper_bound_ = default_receive_size;
};

} // namespace caf::net
