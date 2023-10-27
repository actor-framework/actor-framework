// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/stream_bridge.hpp"

#include "caf/scheduled_actor.hpp"

namespace caf::detail {

namespace {

/// Configures how many (full) batches the bridge must be able to cache at the
/// very least.
constexpr size_t min_batch_buffering = 5;

/// Configures how many batches we request in one go. This is to avoid sending
/// one demand message for each batch we receive.
constexpr size_t min_batch_request_threshold = 3;

} // namespace

void stream_bridge_sub::ack(uint64_t src_flow_id,
                            uint32_t max_items_per_batch) {
  CAF_LOG_TRACE(CAF_ARG(src_flow_id) << CAF_ARG(max_items_per_batch));
  // Sanity checking.
  if (max_items_per_batch == 0) {
    CAF_LOG_ERROR("stream ACK announced a batch size of 0");
    do_abort(make_error(sec::protocol_error));
    return;
  }
  // Update our state. Streams operate on batches, so we translate the
  // user-defined bounds on per-item level to a rough equivalent on batches.
  // Batches may be "under-full", so this isn't perfect in practice.
  src_flow_id_ = src_flow_id;
  max_in_flight_batches_ = std::max(min_batch_buffering,
                                    max_in_flight_ / max_items_per_batch);
  low_batches_threshold_ = std::max(min_batch_request_threshold,
                                    request_threshold_ / max_items_per_batch);
  // Go get some data.
  in_flight_batches_ = max_in_flight_batches_;
  unsafe_send_as(self_, src_,
                 stream_demand_msg{src_flow_id_,
                                   static_cast<uint32_t>(in_flight_batches_)});
}

void stream_bridge_sub::drop() {
  CAF_LOG_TRACE("");
  src_ = nullptr;
  out_.on_complete();
  out_ = nullptr;
}

void stream_bridge_sub::drop(const error& reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  src_ = nullptr;
  out_.on_error(reason);
  out_ = nullptr;
}

void stream_bridge_sub::push(const async::batch& input) {
  CAF_LOG_TRACE(CAF_ARG2("input.size", input.size()));
  // Sanity checking.
  if (in_flight_batches_ == 0) {
    CAF_LOG_ERROR("source exceeded its allowed credit!");
    do_abort(make_error(sec::protocol_error));
    return;
  }
  // Push batch downstream or buffer it.
  --in_flight_batches_;
  if (demand_ > 0) {
    CAF_ASSERT(buf_.empty());
    --demand_;
    out_.on_next(input);
    do_check_credit();
  } else {
    buf_.push_back(input);
  }
}

void stream_bridge_sub::push() {
  CAF_LOG_TRACE("");
  while (!buf_.empty() && demand_ > 0) {
    --demand_;
    out_.on_next(buf_.front());
    buf_.pop_front();
  }
  do_check_credit();
}

bool stream_bridge_sub::disposed() const noexcept {
  return src_ != nullptr;
}

void stream_bridge_sub::dispose() {
  if (!src_)
    return;
  unsafe_send_as(self_, src_, stream_cancel_msg{src_flow_id_});
  auto fn = make_action([self = self_, snk_flow_id = snk_flow_id_] {
    self->drop_flow_state(snk_flow_id);
  });
  self_->delay(std::move(fn));
  src_ = nullptr;
}

void stream_bridge_sub::request(size_t n) {
  demand_ += n;
  if (!buf_.empty()) {
    auto fn = make_action([self = self_, snk_flow_id = snk_flow_id_] {
      self->try_push_stream(snk_flow_id);
    });
    self_->delay(std::move(fn));
  }
}

void stream_bridge_sub::do_abort(const error& reason) {
  auto fn = make_action([self = self_, snk_flow_id = snk_flow_id_] {
    self->drop_flow_state(snk_flow_id);
  });
  self_->delay(std::move(fn));
  out_.on_error(reason);
  out_ = nullptr;
  unsafe_send_as(self_, src_, stream_cancel_msg{src_flow_id_});
  src_ = nullptr;
}

void stream_bridge_sub::do_check_credit() {
  auto capacity = max_in_flight_batches_ - in_flight_batches_ - buf_.size();
  if (capacity >= low_batches_threshold_) {
    in_flight_batches_ += capacity;
    unsafe_send_as(self_, src_,
                   stream_demand_msg{src_flow_id_,
                                     static_cast<uint32_t>(capacity)});
  }
}

stream_bridge::stream_bridge(scheduled_actor* self, strong_actor_ptr src,
                             uint64_t stream_id, size_t buf_capacity,
                             size_t request_threshold)
  : super(self),
    src_(std::move(src)),
    stream_id_(stream_id),
    buf_capacity_(buf_capacity),
    request_threshold_(request_threshold) {
  // nop
}

disposable stream_bridge::subscribe(flow::observer<async::batch> out) {
  if (!src_) {
    return fail_subscription(out, make_error(sec::cannot_resubscribe_stream));
  }
  auto self = self_ptr();
  auto local_id = self->new_u64_id();
  unsafe_send_as(self, src_,
                 stream_open_msg{stream_id_, self->ctrl(), local_id});
  auto sub = make_counted<stream_bridge_sub>(self, std::move(src_), out,
                                             local_id, buf_capacity_,
                                             request_threshold_);
  self->register_flow_state(local_id, sub);
  out.on_subscribe(flow::subscription{sub});
  return sub->as_disposable();
}

scheduled_actor* stream_bridge::self_ptr() {
  // This cast is safe, because the stream_bridge may only be constructed with
  // a scheduled actor pointer.
  return static_cast<scheduled_actor*>(super::ctx());
}

} // namespace caf::detail
