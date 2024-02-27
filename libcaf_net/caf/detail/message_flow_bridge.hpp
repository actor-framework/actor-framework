// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/lp/lower_layer.hpp"
#include "caf/net/lp/upper_layer.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/actor_system.hpp"
#include "caf/async/consumer_adapter.hpp"
#include "caf/async/producer_adapter.hpp"
#include "caf/log/net.hpp"
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
class message_flow_bridge : public lp::upper_layer {
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

  error start(net::socket_manager* mgr, lp::lower_layer* down) {
    down_ = down;
    if (auto in = make_consumer_adapter(in_res_, mgr->mpx_ptr(),
                                        do_wakeup_cb())) {
      in_ = std::move(*in);
      in_res_ = nullptr;
    }
    if (auto out = make_producer_adapter(out_res_, mgr->mpx_ptr(),
                                         do_resume_cb(), do_cancel_cb())) {
      out_ = std::move(*out);
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

  void prepare_send() override {
    input_type tmp;
    while (down_->can_send_more()) {
      switch (in_.pull(async::delay_errors, tmp)) {
        case async::read_result::ok:
          if (!write(tmp)) {
            down_->shutdown(trait_.last_error());
            return;
          }
          break;
        case async::read_result::stop:
          down_->shutdown();
          break;
        case async::read_result::abort:
          down_->shutdown(in_.abort_reason());
          break;
        default: // try later
          return;
      }
    }
  }

  bool done_sending() override {
    return !in_->has_consumer_event();
  }

  void abort(const error& reason) override {
    auto exit_guard = log::net::trace("reason = {}", reason);
    if (out_) {
      if (reason == sec::socket_disconnected || reason == sec::disposed)
        out_.close();
      else
        out_ > abort(reason);
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
  action do_wakeup_cb() {
    return make_action([this] { down_->write_later(); });
  }

  action do_resume_cb() {
    return make_action([this] { down_->request_messages(); });
  }

  action do_cancel_cb() {
    return make_action([this] {
      if (out_) {
        out_.release_later();
        down_->shutdown();
      }
    });
  }

  /// Points to the next layer down the protocol stack.
  lp::lower_layer* down_ = nullptr;

  /// Incoming messages, serialized to the socket.
  async::consumer_adapter<input_type> in_;

  /// Outgoing messages, deserialized from the socket.
  async::producer_adapter<output_type> out_;

  /// Converts between raw bytes and items.
  Trait trait_;

  /// Discarded after initialization.
  pull_resource_t in_res_;

  /// Discarded after initialization.
  push_resource_t out_res_;
};

} // namespace caf::net
