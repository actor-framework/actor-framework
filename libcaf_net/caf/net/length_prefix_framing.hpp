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

  static constexpr size_t max_message_length = INT32_MAX;

  // -- interface for the upper layer ------------------------------------------

  template <class LowerLayer>
  class access {
  public:
    access(LowerLayer* lower_layer, length_prefix_framing* this_layer)
      : lower_layer_(lower_layer), this_layer_(this_layer) {
      // nop
    }

    void begin_message() {
      lower_layer_->begin_output();
      auto& buf = message_buffer();
      message_offset_ = buf.size();
      buf.insert(buf.end(), 4, byte{0});
    }

    byte_buffer& message_buffer() {
      return lower_layer_->output_buffer();
    }

    bool end_message() {
      using detail::to_network_order;
      auto& buf = message_buffer();
      auto msg_begin = buf.begin() + message_offset_;
      auto msg_size = std::distance(msg_begin + 4, buf.end());
      if (msg_size > 0 && msg_size < max_message_length) {
        auto u32_size = to_network_order(static_cast<uint32_t>(msg_size));
        memcpy(std::addressof(*msg_begin), &u32_size, 4);
        return true;
      } else {
        abort_reason(make_error(
          sec::runtime_error, msg_size == 0 ? "logic error: message of size 0"
                                            : "maximum message size exceeded"));
        return false;
      }
    }

    bool can_send_more() const noexcept {
      return lower_layer_->can_send_more();
    }

    void abort_reason(error reason) {
      return lower_layer_->abort_reason(std::move(reason));
    }

    void configure_read(receive_policy policy) {
      lower_layer_->configure_read(policy);
    }

  private:
    LowerLayer* lower_layer_;
    length_prefix_framing* this_layer_;
    size_t message_offset_ = 0;
  };

  // -- properties -------------------------------------------------------------

  auto& upper_layer() noexcept {
    return upper_layer_;
  }

  const auto& upper_layer() const noexcept {
    return upper_layer_;
  }
  // -- role: upper layer ------------------------------------------------------

  template <class LowerLayer>
  bool prepare_send(LowerLayer& down) {
    access<LowerLayer> this_layer{&down, this};
    return upper_layer_.prepare_send(this_layer);
  }

  template <class LowerLayer>
  bool done_sending(LowerLayer& down) {
    access<LowerLayer> this_layer{&down, this};
    return upper_layer_.done_sending(this_layer);
  }

  template <class LowerLayer>
  void abort(LowerLayer& down, const error& reason) {
    access<LowerLayer> this_layer{&down, this};
    return upper_layer_.abort(this_layer, reason);
  }

  template <class LowerLayer>
  ptrdiff_t consume(LowerLayer& down, byte_span buffer, byte_span) {
    using detail::from_network_order;
    if (buffer.size() < 4)
      return 0;
    uint32_t u32_size = 0;
    memcpy(&u32_size, buffer.data(), 4);
    auto msg_size = static_cast<size_t>(from_network_order(u32_size));
    if (buffer.size() < msg_size + 4)
      return 0;
    upper_layer_.consume(down, make_span(buffer.data() + 4, msg_size));
    return msg_size + 4;
  }

private:
  UpperLayer upper_layer_;
};

} // namespace caf::net
