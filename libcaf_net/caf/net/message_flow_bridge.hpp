// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/net/consumer_adapter.hpp"
#include "caf/net/message_oriented.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/producer_adapter.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/sec.hpp"
#include "caf/settings.hpp"

#include <utility>

namespace caf::net {

/// Translates between a message-oriented transport and data flows.
///
/// The `Trait` provides a customization point that converts between native and
/// wire format.
///
/// ~~~
/// class my_trait {
///   using input_type = ...;
///   using output_type = ...;
///   bool convert(const_byte_span bytes, input_type& value);
///   bool convert(const output_type& value, byte_buffer& bytes);
/// };
/// ~~~
template <class Trait>
class message_flow_bridge : public message_oriented::upper_layer {
public:
  /// The input type for the application.
  using input_type = typename Trait::input_type;

  /// The output type for of application.
  using output_type = typename Trait::output_type;

  /// The resource type we pull from. We consume the output of the application.
  using pull_resource_t = async::consumer_resource<output_type>;

  /// The buffer type from the @ref pull_resource_t.
  using pull_buffer_t = async::spsc_buffer<output_type>;

  /// Type for the producer adapter. We produce the input of the application.
  using push_resource_t = async::producer_resource<input_type>;

  /// The buffer type from the @ref push_resource_t.
  using push_buffer_t = async::spsc_buffer<input_type>;

  message_flow_bridge(pull_resource_t in_res, push_resource_t out_res,
                      Trait trait)
    : in_res_(std::move(in_res)),
      trait_(std::move(trait)),
      out_res_(std::move(out_res)) {
    // nop
  }

  explicit message_flow_bridge(Trait trait) : trait_(std::move(trait)) {
    // nop
  }

  error init(net::socket_manager* mgr, message_oriented::lower_layer* down,
             const settings&) {
    down_ = down;
    if (in_res_) {
      in_ = consumer_adapter<pull_buffer_t>::try_open(mgr, in_res_);
      in_res_ = nullptr;
    }
    if (out_res_) {
      out_ = producer_adapter<push_buffer_t>::try_open(mgr, out_res_);
      out_res_ = nullptr;
    }
    if (!in_ && !out_)
      return make_error(sec::cannot_open_resource,
                        "a flow bridge needs at least one valid resource");
    return none;
  }

  bool write(const input_type& item) {
    down_->begin_message();
    auto& buf = down_->message_buffer();
    return trait_.convert(item, buf) && down_->end_message();
  }

  struct write_helper {
    message_flow_bridge* bridge;
    bool aborted = false;
    error err;

    write_helper(message_flow_bridge* bridge) : bridge(bridge) {
      // nop
    }

    void on_next(const input_type& item) {
      if (!bridge->write(item))
        aborted = true;
    }

    void on_complete() {
      // nop
    }

    void on_error(const error& x) {
      err = x;
    }
  };

  bool prepare_send() override {
    write_helper helper{this};
    while (down_->can_send_more() && in_) {
      auto [again, consumed] = in_->pull(async::delay_errors, 1, helper);
      if (!again) {
        if (helper.err) {
          down_->send_close_message(helper.err);
        } else {
          down_->send_close_message();
        }
        in_ = nullptr;
      } else if (helper.aborted) {
        in_->cancel();
        in_ = nullptr;
        return false;
      } else if (consumed == 0) {
        return true;
      }
    }
    return true;
  }

  bool done_sending() override {
    return !in_ || !in_->has_data();
  }

  void abort(const error& reason) override {
    CAF_LOG_TRACE(CAF_ARG(reason));
    if (out_) {
      if (reason == sec::socket_disconnected || reason == sec::disposed)
        out_->close();
      else
        out_->abort(reason);
      out_ = nullptr;
    }
    if (in_) {
      in_->cancel();
      in_ = nullptr;
    }
  }

  ptrdiff_t consume(byte_span buf) {
    if (!out_)
      return -1;
    input_type val;
    if (!trait_.convert(buf, val))
      return -1;
    if (out_->push(std::move(val)) == 0)
      down_->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }

private:
  /// Points to the next layer down the protocol stack.
  message_oriented::lower_layer* down_ = nullptr;

  /// Incoming messages, serialized to the socket.
  consumer_adapter_ptr<pull_buffer_t> in_;

  /// Outgoing messages, deserialized from the socket.
  producer_adapter_ptr<push_buffer_t> out_;

  /// Converts between raw bytes and items.
  Trait trait_;

  /// Discarded after initialization.
  pull_resource_t in_res_;

  /// Discarded after initialization.
  push_resource_t out_res_;
};

} // namespace caf::net
