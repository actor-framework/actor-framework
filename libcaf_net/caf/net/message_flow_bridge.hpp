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
#include "caf/tag/mixed_message_oriented.hpp"
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
template <class T, class Trait, class Tag = tag::message_oriented>
class message_flow_bridge : public tag::no_auto_reading {
public:
  using input_tag = Tag;

  using buffer_type = async::spsc_buffer<T>;

  using consumer_resource_t = async::consumer_resource<T>;

  using producer_resource_t = async::producer_resource<T>;

  explicit message_flow_bridge(Trait trait) : trait_(std::move(trait)) {
    // nop
  }

  void connect_flows(net::socket_manager* mgr, consumer_resource_t in,
                     producer_resource_t out) {
    in_ = consumer_adapter<buffer_type>::try_open(mgr, in);
    out_ = producer_adapter<buffer_type>::try_open(mgr, out);
  }

  template <class LowerLayerPtr>
  error
  init(net::socket_manager* mgr, LowerLayerPtr down, const settings& cfg) {
    mgr_ = mgr;
    if constexpr (caf::detail::has_init_v<Trait>) {
      if (auto err = init_res(trait_.init(cfg)))
        return err;
    }
    if (!in_ && !out_)
      return make_error(sec::cannot_open_resource,
                        "flow bridge cannot run without at least one resource");
    if (!out_)
      down->suspend_reading();
    return none;
  }

  template <class LowerLayerPtr>
  bool write(LowerLayerPtr down, const T& item) {
    if constexpr (std::is_same_v<Tag, tag::message_oriented>) {
      down->begin_message();
      auto& buf = down->message_buffer();
      return trait_.convert(item, buf) && down->end_message();
    } else {
      static_assert(std::is_same_v<Tag, tag::mixed_message_oriented>);
      if (trait_.converts_to_binary(item)) {
        down->begin_binary_message();
        auto& buf = down->binary_message_buffer();
        return trait_.convert(item, buf) && down->end_binary_message();
      } else {
        down->begin_text_message();
        auto& buf = down->text_message_buffer();
        return trait_.convert(item, buf) && down->end_text_message();
      }
    }
  }

  template <class LowerLayerPtr>
  struct write_helper {
    using bridge_type = message_flow_bridge;
    bridge_type* bridge;
    LowerLayerPtr down;
    bool aborted = false;
    size_t consumed = 0;
    error err;

    write_helper(bridge_type* bridge, LowerLayerPtr down)
      : bridge(bridge), down(down) {
      // nop
    }

    void on_next(span<const T> items) {
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

    void on_error(const error& x) {
      err = x;
    }
  };

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr down) {
    write_helper<LowerLayerPtr> helper{this, down};
    while (down->can_send_more() && in_) {
      auto [again, consumed] = in_->pull(async::delay_errors, 1, helper);
      if (!again) {
        if (helper.err) {
          down->send_close_message(helper.err);
        } else {
          down->send_close_message();
        }
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
  void abort(LowerLayerPtr, const error& reason) {
    CAF_LOG_TRACE(CAF_ARG(reason));
    if (out_) {
      if (reason == sec::socket_disconnected || reason == sec::discarded)
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

  template <class U = Tag, class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr down, byte_span buf) {
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

  template <class U = Tag, class LowerLayerPtr>
  ptrdiff_t consume_binary(LowerLayerPtr down, byte_span buf) {
    return consume(down, buf);
  }

  template <class U = Tag, class LowerLayerPtr>
  ptrdiff_t consume_text(LowerLayerPtr down, string_view buf) {
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
  error init_res(error err) {
    return err;
  }

  error init_res(consumer_resource_t in, producer_resource_t out) {
    connect_flows(mgr_, std::move(in), std::move(out));
    return caf::none;
  }

  error init_res(std::tuple<consumer_resource_t, producer_resource_t> in_out) {
    auto& [in, out] = in_out;
    return init_res(std::move(in), std::move(out));
  }

  error init_res(std::pair<consumer_resource_t, producer_resource_t> in_out) {
    auto& [in, out] = in_out;
    return init_res(std::move(in), std::move(out));
  }

  template <class R>
  error init_res(expected<R> res) {
    if (res)
      return init_res(*res);
    else
      return std::move(res.error());
  }

  /// Points to the manager that runs this protocol stack.
  net::socket_manager* mgr_ = nullptr;

  /// Incoming messages, serialized to the socket.
  consumer_adapter_ptr<buffer_type> in_;

  /// Outgoing messages, deserialized from the socket.
  producer_adapter_ptr<buffer_type> out_;

  /// Converts between raw bytes and items.
  Trait trait_;
};

} // namespace caf::net
