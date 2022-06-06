// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/consumer_adapter.hpp"
#include "caf/async/producer_adapter.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/fwd.hpp"
#include "caf/net/flow_connector.hpp"
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
  using consumer_type = async::consumer_adapter<output_type>;

  /// Type for the producer adapter. We produce the input of the application.
  using producer_type = async::producer_adapter<input_type>;

  using request_type = request<Trait>;

  using connector_pointer = flow_connector_ptr<Trait>;

  explicit flow_bridge(async::execution_context_ptr loop,
                       connector_pointer conn)
    : loop_(std::move(loop)), conn_(std::move(conn)) {
    // nop
  }

  static std::unique_ptr<flow_bridge> make(async::execution_context_ptr loop,
                                           connector_pointer conn) {
    return std::make_unique<flow_bridge>(std::move(loop), std::move(conn));
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

  bool running() const noexcept {
    return in_ || out_;
  }

  // -- implementation of web_socket::lower_layer ------------------------------

  error start(web_socket::lower_layer* down, const settings& cfg) override {
    down_ = down;
    auto [err, pull, push] = conn_->on_request(cfg);
    if (!err) {
      auto do_wakeup = make_action([this] {
        prepare_send();
        if (!running())
          down_->shutdown();
      });
      auto do_resume = make_action([this] { down_->request_messages(); });
      auto do_cancel = make_action([this] {
        if (!running())
          down_->shutdown();
      });
      in_ = consumer_type::make(pull.try_open(), loop_, std::move(do_wakeup));
      out_ = producer_type::make(push.try_open(), loop_, std::move(do_resume),
                                 std::move(do_cancel));
      conn_ = nullptr;
      if (running())
        return none;
      else
        return make_error(sec::runtime_error,
                          "cannot init flow bridge: no buffers");
    } else {
      conn_ = nullptr;
      return err;
    }
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
    return !in_.has_consumer_event();
  }

  void abort(const error& reason) override {
    CAF_LOG_TRACE(CAF_ARG(reason));
    if (out_) {
      if (reason == sec::connection_closed || reason == sec::socket_disconnected
          || reason == sec::disposed)
        out_.close();
      else
        out_.abort(reason);
    }
    in_.cancel();
  }

  ptrdiff_t consume_binary(byte_span buf) override {
    if (!out_)
      return -1;
    input_type val;
    if (!trait_.convert(buf, val))
      return -1;
    if (out_.push(std::move(val)) == 0)
      down_->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }

  ptrdiff_t consume_text(std::string_view buf) override {
    if (!out_)
      return -1;
    input_type val;
    if (!trait_.convert(buf, val))
      return -1;
    if (out_.push(std::move(val)) == 0)
      down_->suspend_reading();
    return static_cast<ptrdiff_t>(buf.size());
  }

private:
  web_socket::lower_layer* down_;

  /// The output of the application. Serialized to the socket.
  consumer_type in_;

  /// The input to the application. Deserialized from the socket.
  producer_type out_;

  /// Converts between raw bytes and native C++ objects.
  Trait trait_;

  /// Runs callbacks in the I/O event loop.
  async::execution_context_ptr loop_;

  /// Initializes the bridge. Disposed (set to null) after initializing.
  connector_pointer conn_;
};

} // namespace caf::net::web_socket
