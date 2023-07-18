// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/http/request_header.hpp"

#include "caf/async/consumer_adapter.hpp"
#include "caf/async/producer_adapter.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"

#include <utility>

namespace caf::detail {

/// Translates between a message-oriented transport and data flows.
template <class UpperLayer, class LowerLayer, class Trait>
class flow_bridge_base : public UpperLayer {
public:
  using input_type = typename Trait::input_type;

  using output_type = typename Trait::output_type;

  /// Type for the consumer adapter. We consume the output of the application.
  using consumer_type = async::consumer_adapter<output_type>;

  /// Type for the producer adapter. We produce the input of the application.
  using producer_type = async::producer_adapter<input_type>;

  virtual bool write(const output_type& item) = 0;

  bool running() const noexcept {
    return in_ && out_;
  }

  /// Initializes consumer and producer of the bridge.
  error init(net::multiplexer* mpx, async::consumer_resource<output_type> pull,
             async::producer_resource<input_type> push) {
    // Initialize our consumer.
    auto do_wakeup = make_action([this] {
      if (running())
        prepare_send();
    });
    in_ = consumer_type::make(pull.try_open(), mpx, std::move(do_wakeup));
    if (!in_) {
      auto err = make_error(sec::runtime_error,
                            "flow bridge failed to open the input resource");
      push.abort(err);
      return err;
    }
    // Initialize our producer.
    auto do_resume = make_action([this] { down_->request_messages(); });
    auto do_cancel = make_action([this] {
      if (!running()) {
        down_->shutdown();
      }
    });
    out_ = producer_type::make(push.try_open(), mpx, std::move(do_resume),
                               std::move(do_cancel));
    if (!out_) {
      auto err = make_error(sec::runtime_error,
                            "flow bridge failed to open the output resource");
      in_.cancel();
      return err;
    }
    return {};
  }

  void self_ref(disposable ref) {
    self_ref_ = std::move(ref);
  }

  // -- implementation of the lower_layer --------------------------------------

  void prepare_send() override {
    input_type tmp;
    while (down_->can_send_more()) {
      switch (in_.pull(async::delay_errors, tmp)) {
        case async::read_result::ok:
          if (!write(tmp)) {
            abort(trait_.last_error());
            down_->shutdown(trait_.last_error());
            return;
          }
          break;
        case async::read_result::stop:
          in_ = nullptr;
          abort(error{});
          down_->shutdown();
          return;
        case async::read_result::abort:
          in_ = nullptr;
          abort(in_.abort_reason());
          down_->shutdown(in_.abort_reason());
          return;
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
      if (!reason || reason == sec::connection_closed
          || reason == sec::socket_disconnected || reason == sec::disposed)
        out_.close();
      else
        out_.abort(reason);
      out_ = nullptr;
    }
    if (in_) {
      in_.cancel();
      in_ = nullptr;
    }
    self_ref_ = nullptr;
  }

protected:
  LowerLayer* down_;

  /// The output of the application. Serialized to the socket.
  consumer_type in_;

  /// The input to the application. Deserialized from the socket.
  producer_type out_;

  /// Converts between raw bytes and native C++ objects.
  Trait trait_;

  /// Type-erased handle to the @ref socket_manager. This reference is important
  /// to keep the bridge alive while the manager is not registered for writing
  /// or reading.
  disposable self_ref_;
};

} // namespace caf::detail
