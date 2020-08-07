/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <memory>

#include "caf/byte.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/sec.hpp"
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

  constexpr size_t max_message_length = INT32_MAX;

  // -- interface for the upper layer ------------------------------------------

  template <class LowerLayer>
  class access {
  public:
    access(LowerLayer* lower_layer, length_prefix_framing* this_layer)
      : lower_layer_(lower_layer), this_layer(this_layer) {
      // nop
    }

    void begin_message() {
      lower_layer_->begin_output();
      auto& buf = message_buffer();
      message_offset_ = buf.size();
      buf.insert(buf.end(), 4, byte{0});
    }

    byte_buffer& message_buffer() {
      return lower_layer_->output_buffer.size();
    }

    void end_message() {
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
      if (policy.max_size > 0 && transport_->max_read_size_ == 0)
        parent_->register_reading();
      transport_->min_read_size_ = policy.min_size;
      transport_->max_read_size_ = policy.max_size;
      transport_->read_buf_.resize(policy.max_size);
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
  ptrdiff_t consume(LowerLayer& down, byte_span buffer, byte_span delta) {
  }

private:
  UpperLayer upper_layer_;
};

} // namespace caf::net
