// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/fwd.hpp"
#include "caf/net/consumer_adapter.hpp"
#include "caf/net/producer_adapter.hpp"
#include "caf/net/web_socket/flow_connector.hpp"
#include "caf/net/web_socket/lower_layer.hpp"
#include "caf/net/web_socket/request.hpp"
#include "caf/net/web_socket/upper_layer.hpp"
#include "caf/sec.hpp"

#include <utility>

namespace caf::net::web_socket {

/// Translates between a message-oriented transport and data flows.
template <class Trait>
class flow_bridge : public web_socket::upper_layer {
public:
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

  static std::unique_ptr<flow_bridge> make(connector_pointer conn) {
    return std::make_unique<flow_bridge>(std::move(conn));
  }

  bool write(const output_type& item) {
    if (trait_.converts_to_binary(item)) {
      down_->begin_binary_message();
      auto& bytes = down_->binary_message_buffer();
      return trait_.convert(item, bytes) && down_->end_binary_message();
    } else {
      down_->begin_text_message();
      auto& text = down_->text_message_buffer();
      return trait_.convert(item, text) && down_->end_text_message();
    }
  }

  struct write_helper {
    flow_bridge* thisptr;
    bool aborted = false;
    error err;

    explicit write_helper(flow_bridge* thisptr) : thisptr(thisptr) {
      // nop
    }

    void on_next(const output_type& item) {
      if (!thisptr->write(item))
        aborted = true;
    }

    void on_complete() {
      // nop
    }

    void on_error(const error& x) {
      err = x;
    }
  };

  // -- implementation of web_socket::lower_layer ------------------------------

  error init(net::socket_manager* mgr, web_socket::lower_layer* down,
             const settings& cfg) override {
    down_ = down;
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

  void continue_reading() override {
    // nop
  }

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
        return false;
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

  ptrdiff_t consume_binary(byte_span buf) override {
    if (!out_)
      return -1;
    input_type val;
    if (!trait_.convert(buf, val))
      return -1;
    if (out_->push(std::move(val)) == 0)
      down_->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }

  ptrdiff_t consume_text(std::string_view buf) override {
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
  web_socket::lower_layer* down_;

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
