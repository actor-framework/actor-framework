// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/octet_stream/flow_bridge.hpp"

namespace caf::detail {

flow::coordinator* octet_stream_observer::parent() const noexcept {
  return parent_;
}

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

} // namespace caf::detail

namespace caf::net::octet_stream {

void flow_bridge::on_subscribed(ucast_sub_state*) {
  down_->configure_read(receive_policy::up_to(read_buffer_size_));
}

void flow_bridge::on_disposed(ucast_sub_state*, bool from_external) {
  if (from_external) {
    self_->schedule_fn([this] { on_disposed(nullptr, false); });
    return;
  }
  down_->shutdown();
}

void flow_bridge::on_consumed_some(ucast_sub_state*, size_t,
                                   size_t new_buffer_size) {
  if (new_buffer_size < read_buffer_size_) {
    auto delta = static_cast<uint32_t>(read_buffer_size_ - new_buffer_size);
    down_->configure_read(receive_policy::up_to(delta));
  }
}

void flow_bridge::init(socket_manager* ptr) {
  self_ = ptr;
  in_ = make_counted<flow::op::ucast<std::byte>>(ptr);
  in_->state().listener = this;
}

error flow_bridge::start(lower_layer* down) {
  down_ = down;
  on_start();
  return none;
}

void flow_bridge::prepare_send() {
  // nop
}

bool flow_bridge::done_sending() {
  return true;
}

void flow_bridge::abort(const error& reason) {
  in_->state().abort(reason);
  out_.cancel();
}

ptrdiff_t flow_bridge::consume(byte_span buf, byte_span) {
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

void flow_bridge::written(size_t num_bytes) {
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

void flow_bridge::on_next(std::byte item) {
  if (requested_ > 0)
    --requested_;
  else
    ++overflow_;
  down_->begin_output();
  down_->output_buffer().push_back(item);
  down_->end_output();
}

void flow_bridge::on_error(const error& what) {
  abort(what);
  out_.release_later();
}

void flow_bridge::on_complete() {
  out_.release_later();
}

void flow_bridge::on_subscribe(flow::subscription sub) {
  if (out_) {
    sub.cancel();
    return;
  }
  out_ = std::move(sub);
  out_.request(write_buffer_size_);
  requested_ = write_buffer_size_;
}

} // namespace caf::net::octet_stream
