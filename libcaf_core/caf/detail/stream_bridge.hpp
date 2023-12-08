// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/hot.hpp"
#include "caf/flow/subscription.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>

namespace caf::detail {

class stream_bridge_sub : public flow::subscription::impl_base {
public:
  stream_bridge_sub(scheduled_actor* self, strong_actor_ptr src,
                    flow::observer<async::batch> out, uint64_t snk_flow_id,
                    size_t max_in_flight, size_t request_threshold)
    : self_(self),
      src_(std::move(src)),
      out_(std::move(out)),
      snk_flow_id_(snk_flow_id),
      max_in_flight_(max_in_flight),
      request_threshold_(request_threshold) {
    // nop
  }

  // -- callbacks for the actor ------------------------------------------------

  void ack(uint64_t src_flow_id, uint32_t max_items_per_batch);

  void drop();

  void drop(const error& reason);

  void push(const async::batch& input);

  void push();

  // -- implementation of subscription -----------------------------------------

  flow::coordinator* parent() const noexcept override;

  bool disposed() const noexcept override;

  void request(size_t n) override;

private:
  void do_dispose(bool from_external) override;

  bool initialized() const noexcept {
    return src_flow_id_ != 0;
  }

  void do_abort(const error& reason);

  void do_check_credit();

  scheduled_actor* self_;
  strong_actor_ptr src_;

  flow::observer<async::batch> out_;

  uint64_t src_flow_id_ = 0;
  uint64_t snk_flow_id_;

  size_t max_in_flight_batches_ = 0;
  size_t in_flight_batches_ = 0;
  size_t low_batches_threshold_ = 0;

  size_t demand_ = 0;
  std::deque<async::batch> buf_;

  size_t max_in_flight_;
  size_t request_threshold_;
};

using stream_bridge_sub_ptr = intrusive_ptr<stream_bridge_sub>;

class stream_bridge : public flow::op::hot<async::batch> {
public:
  using super = flow::op::hot<async::batch>;

  explicit stream_bridge(scheduled_actor* self, strong_actor_ptr src,
                         uint64_t stream_id, size_t buf_capacity,
                         size_t request_threshold);

  disposable subscribe(flow::observer<async::batch> out) override;

private:
  scheduled_actor* self_ptr();

  strong_actor_ptr src_;
  uint64_t stream_id_;
  size_t buf_capacity_;
  size_t request_threshold_;
};

} // namespace caf::detail
