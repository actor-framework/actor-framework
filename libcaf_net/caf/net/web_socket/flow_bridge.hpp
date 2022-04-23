// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/fwd.hpp"
#include "caf/net/consumer_adapter.hpp"
#include "caf/net/producer_adapter.hpp"
#include "caf/net/web_socket/flow_connector.hpp"
#include "caf/net/web_socket/request.hpp"
#include "caf/sec.hpp"
#include "caf/tag/mixed_message_oriented.hpp"
#include "caf/tag/no_auto_reading.hpp"

#include <utility>

namespace caf::net::web_socket {

/// Translates between a message-oriented transport and data flows.
template <class Trait>
class flow_bridge : public tag::no_auto_reading {
public:
  using input_tag = tag::mixed_message_oriented;

  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  /// Type for the consumer adapter. We consume the output of the application.
  using consumer_type = consumer_adapter<async::spsc_buffer<output_type>>;

  /// Type for the producer adapter. We produce the input of the application.
  using producer_type = producer_adapter<async::spsc_buffer<input_type>>;

  using request_type = request<Trait>;

  using connector_pointer = flow_connector_ptr<Trait>;

  explicit flow_bridge(connector_pointer conn) : conn_(std::move(conn)) {
    // nop
  }

  template <class LowerLayerPtr>
  error init(net::socket_manager* mgr, LowerLayerPtr, const settings& cfg) {
    auto [err, pull, push] = conn_->on_request(cfg);
    if (!err) {
      in_ = consumer_type::try_open(mgr, pull);
      out_ = producer_type::try_open(mgr, push);
      CAF_ASSERT(in_ != nullptr);
      CAF_ASSERT(out_ != nullptr);
      conn_ = nullptr;
      return none;
    } else {
      conn_ = nullptr;
      return err;
    }
  }

  template <class LowerLayerPtr>
  bool write(LowerLayerPtr down, const output_type& item) {
    if (trait_.converts_to_binary(item)) {
      down->begin_binary_message();
      auto& bytes = down->binary_message_buffer();
      return trait_.convert(item, bytes) && down->end_binary_message();
    } else {
      down->begin_text_message();
      auto& text = down->text_message_buffer();
      return trait_.convert(item, text) && down->end_text_message();
    }
  }

  template <class LowerLayerPtr>
  struct write_helper {
    using bridge_type = flow_bridge;
    bridge_type* bridge;
    LowerLayerPtr down;
    bool aborted = false;
    size_t consumed = 0;
    error err;

    write_helper(bridge_type* bridge, LowerLayerPtr down)
      : bridge(bridge), down(down) {
      // nop
    }

    void on_next(const output_type& item) {
      if (!bridge->write(down, item)) {
        aborted = true;
        return;
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

  template <class LowerLayerPtr>
  ptrdiff_t consume_binary(LowerLayerPtr down, byte_span buf) {
    if (!out_) {
      down->abort_reason(make_error(sec::connection_closed));
      return -1;
    }
    input_type val;
    if (!trait_.convert(buf, val)) {
      down->abort_reason(make_error(sec::conversion_failed));
      return -1;
    }
    if (out_->push(std::move(val)) == 0)
      down->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume_text(LowerLayerPtr down, std::string_view buf) {
    if (!out_) {
      down->abort_reason(make_error(sec::connection_closed));
      return -1;
    }
    input_type val;
    if (!trait_.convert(buf, val)) {
      down->abort_reason(make_error(sec::conversion_failed));
      return -1;
    }
    if (out_->push(std::move(val)) == 0)
      down->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }

private:
  /// The output of the application. Serialized to the socket.
  intrusive_ptr<consumer_type> in_;

  /// The input to the application. Deserialized from the socket.
  intrusive_ptr<producer_type> out_;

  /// Converts between raw bytes and native C++ objects.
  Trait trait_;

  /// Initializes the bridge. Disposed (set to null) after initializing.
  connector_pointer conn_;
};

} // namespace caf::net::web_socket
