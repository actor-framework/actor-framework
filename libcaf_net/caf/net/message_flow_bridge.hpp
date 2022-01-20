// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/net/consumer_adapter.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/producer_adapter.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/sec.hpp"
#include "caf/settings.hpp"
#include "caf/tag/message_oriented.hpp"
#include "caf/tag/no_auto_reading.hpp"

#include <utility>

namespace caf::net {

/// Translates between a message-oriented transport and data flows.
///
/// The trait class converts between the native and the wire format:
///
/// ~~~
/// struct my_trait {
///   bool convert(const T& value, byte_buffer& bytes);
///   bool convert(const_byte_span bytes, T& value);
/// };
/// ~~~
template <class T, class Trait>
class message_flow_bridge : public caf::tag::no_auto_reading {
public:
  using input_tag = caf::tag::message_oriented;

  using buffer_type = caf::async::spsc_buffer<T>;

  explicit message_flow_bridge(Trait trait) : trait_(std::move(trait)) {
    // nop
  }

  void connect_flows(caf::net::socket_manager* mgr,
                     async::consumer_resource<T> in,
                     async::producer_resource<T> out) {
    in_ = consumer_adapter<buffer_type>::try_open(mgr, in);
    out_ = producer_adapter<buffer_type>::try_open(mgr, out);
  }

  template <class LowerLayerPtr>
  caf::error
  init(caf::net::socket_manager* mgr, LowerLayerPtr&&, const caf::settings&) {
    mgr_ = mgr;
    if (!in_ && !out_)
      return make_error(sec::cannot_open_resource,
                        "flow bridge cannot run without at least one resource");
    else
      return caf::none;
  }

  template <class LowerLayerPtr>
  bool write(LowerLayerPtr down, const T& item) {
    down->begin_message();
    auto& buf = down->message_buffer();
    return trait_.convert(item, buf) && down->end_message();
  }

  template <class LowerLayerPtr>
  struct send_helper {
    using bridge_type = message_flow_bridge;
    bridge_type* bridge;
    LowerLayerPtr down;
    bool aborted = false;
    size_t consumed = 0;

    send_helper(bridge_type* bridge, LowerLayerPtr down)
      : bridge(bridge), down(down) {
      // nop
    }

    void on_next(caf::span<const T> items) {
      CAF_ASSERT(items.size() == 1);
      for (const auto& item : items) {
        if (!bridge->write(down, item)) {
          aborted = true;
          return;
        }
      }
    }

    void on_complete() {
      // nop
    }

    void on_error(const caf::error&) {
      // nop
    }
  };

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr down) {
    send_helper<LowerLayerPtr> helper{this, down};
    while (down->can_send_more() && in_) {
      auto [again, consumed] = in_->pull(caf::async::delay_errors, 1, helper);
      if (!again) {
        in_ = nullptr;
      } else if (helper.aborted) {
        down->abort_reason(make_error(sec::conversion_failed));
        in_->cancel();
        in_ = nullptr;
        return false;
      } else if (consumed == 0) {
        return true;
      }
    }
    return true;
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr) {
    return !in_ || !in_->has_data();
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr, const caf::error& reason) {
    CAF_LOG_TRACE(CAF_ARG(reason));
    if (out_) {
      if (reason == caf::sec::socket_disconnected
          || reason == caf::sec::discarded)
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

  template <class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr down, caf::byte_span buf) {
    if (!out_) {
      down->abort_reason(make_error(sec::connection_closed));
      return -1;
    }
    T val;
    if (!trait_.convert(buf, val)) {
      down->abort_reason(make_error(sec::conversion_failed));
      return -1;
    }
    if (out_->push(std::move(val)) == 0)
      down->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }

private:
  /// Points to the manager that runs this protocol stack.
  caf::net::socket_manager* mgr_ = nullptr;

  /// Incoming messages, serialized to the socket.
  consumer_adapter_ptr<buffer_type> in_;

  /// Outgoing messages, deserialized from the socket.
  producer_adapter_ptr<buffer_type> out_;

  /// Converts between raw bytes and items.
  Trait trait_;
};

} // namespace caf::net
