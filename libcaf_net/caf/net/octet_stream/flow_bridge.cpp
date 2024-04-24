// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/octet_stream/flow_bridge.hpp"

#include "caf/net/octet_stream/lower_layer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/op/ucast.hpp"
#include "caf/flow/subscription.hpp"

namespace caf::net::octet_stream {

namespace {

using ucast_sub_state = flow::op::ucast_sub_state<std::byte>;

class flow_bridge;

/// Trivial observer that forwards all events to a
/// `net::octet_stream::flow_bridge`.
class octet_stream_observer : public flow::observer_impl_base<std::byte> {
public:
  using input_type = std::byte;

  octet_stream_observer(flow::coordinator* parent, flow_bridge* listener)
    : parent_(parent), listener_(listener) {
    // nop
  }

  flow::coordinator* parent() const noexcept override {
    return parent_;
  }

  void on_next(const std::byte& item) override;

  void on_error(const error&) override;

  void on_complete() override;

  void on_subscribe(flow::subscription new_sub) override;

private:
  flow::coordinator* parent_;
  flow_bridge* listener_;
};

/// Translates between a byte-oriented transport and data flows. Utility class
/// for the `with` DSL.
class flow_bridge : public upper_layer,
                    public ucast_sub_state::abstract_listener {
public:
  flow_bridge(uint32_t read_buffer_size, uint32_t write_buffer_size,
              detail::flow_bridge_initializer_ptr init)
    : read_buffer_size_(read_buffer_size),
      write_buffer_size_(write_buffer_size),
      init_(std::move(init)) {
    // nop
  }

  void on_subscribed(ucast_sub_state*) override {
    down_->configure_read(receive_policy::up_to(read_buffer_size_));
  }

  void on_disposed(ucast_sub_state*, bool from_external) override {
    if (from_external) {
      self_->schedule_fn([this] { on_disposed(nullptr, false); });
      return;
    }
    down_->shutdown();
  }

  void on_consumed_some(ucast_sub_state*, size_t,
                        size_t new_buffer_size) override {
    if (new_buffer_size < read_buffer_size_) {
      auto delta = static_cast<uint32_t>(read_buffer_size_ - new_buffer_size);
      down_->configure_read(receive_policy::up_to(delta));
    }
  }

  error start(lower_layer* down) override {
    down_ = down;
    self_ = down->manager();
    in_ = make_counted<flow::op::ucast<std::byte>>(self_);
    in_->state().listener = this;
    auto self = static_cast<flow::coordinator*>(self_);
    auto obs_ptr = make_counted<octet_stream_observer>(self, this);
    init_->init_outputs(self, flow::observer<std::byte>{std::move(obs_ptr)});
    init_->init_inputs(self, flow::observable<std::byte>{in_});
    init_ = nullptr;
    return none;
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  void abort(const error& reason) override {
    in_->state().abort(reason);
    out_.cancel();
  }

  ptrdiff_t consume(byte_span buf, byte_span) override {
    if (!in_) {
      CAF_LOG_DEBUG("flow_bridge::consume: !in_");
      return -1;
    }
    auto& st = in_->state();
    if (st.disposed) {
      CAF_LOG_DEBUG("flow_bridge::consume: st.disposed");
      return -1;
    }
    for (auto val : buf) {
      // Note: we can safely ignore the return value here, because buffering the
      //       values is fine. We tie the buffer size to the read buffer size,
      //       which means we can't overflow the buffer.
      std::ignore = st.push(val);
    }
    return static_cast<ptrdiff_t>(buf.size());
  }

  void written(size_t num_bytes) override {
    if (!out_)
      return;
    if (overflow_ > 0) {
      auto delta = std::min(overflow_, num_bytes);
      overflow_ -= delta;
      num_bytes -= delta;
    }
    if (num_bytes > 0) {
      out_.request(num_bytes);
      requested_ += num_bytes;
    }
  }

  void on_next(std::byte item) {
    if (requested_ > 0)
      --requested_;
    else
      ++overflow_;
    down_->begin_output();
    down_->output_buffer().push_back(item);
    down_->end_output();
  }

  void on_error(const error& what) {
    abort(what);
    out_.release_later();
  }

  void on_complete() {
    out_.release_later();
  }

  void on_subscribe(flow::subscription sub) {
    if (out_) {
      sub.cancel();
      return;
    }
    out_ = std::move(sub);
    out_.request(write_buffer_size_);
    requested_ = write_buffer_size_;
  }

protected:
  /// The socket manager that owns this flow bridge.
  socket_manager* self_ = nullptr;

  /// The maximum size of the read buffer.
  uint32_t read_buffer_size_ = 0;

  /// The maximum size of the write buffer.
  uint32_t write_buffer_size_ = 0;

  /// Points to the next layer down the protocol stack.
  lower_layer* down_ = nullptr;

  /// The flow that consumes the bytes we receive from the lower layer.
  flow::op::ucast_ptr<std::byte> in_;

  /// The subscription for the flow that generates the bytes to send.
  flow::subscription out_;

  /// Stores how many bytes we have requested from `out_`.
  size_t requested_ = 0;

  /// Stores excess bytes from `out_` that exceeded the assigned capacity.
  size_t overflow_ = 0;

  detail::flow_bridge_initializer_ptr init_;
};

void octet_stream_observer::on_next(const std::byte& item) {
  if (listener_)
    listener_->on_next(item);
}

void octet_stream_observer::on_error(const error& what) {
  if (listener_) {
    listener_->on_error(what);
    listener_ = nullptr;
  }
}

void octet_stream_observer::on_complete() {
  if (listener_) {
    listener_->on_complete();
    listener_ = nullptr;
  }
}

void octet_stream_observer::on_subscribe(flow::subscription new_sub) {
  if (listener_)
    listener_->on_subscribe(std::move(new_sub));
}

} // namespace

std::unique_ptr<upper_layer>
make_flow_bridge(uint32_t read_buffer_size, uint32_t write_buffer_size,
                 detail::flow_bridge_initializer_ptr init) {
  return std::make_unique<flow_bridge>(read_buffer_size, write_buffer_size,
                                       std::move(init));
}

} // namespace caf::net::octet_stream
