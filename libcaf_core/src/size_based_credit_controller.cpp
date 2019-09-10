/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/size_based_credit_controller.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/serialized_size.hpp"
#include "caf/scheduled_actor.hpp"

// Safe us some typing and very ugly formatting.
#define impl size_based_credit_controller

namespace caf {
namespace detail {

impl::impl(scheduled_actor* self) : super(self) {
  auto& cfg = self->system().config();
  complexity_ = cfg.stream_desired_batch_complexity;
  buffer_capacity_ = get_or(cfg, "stream.size-policy.buffer-capacity",
                            defaults::stream::size_policy::buffer_capacity);
  bytes_per_batch_ = get_or(cfg, "stream.size-policy.bytes-per-batch",
                            defaults::stream::size_policy::bytes_per_batch);
}

impl::~impl() {
  // nop
}

void impl::before_processing(downstream_msg::batch& x) {
  num_elements_ += x.xs_size;
  total_size_ = serialized_size(self()->system(), x.xs);
  processing_begin_ = clock_type::now();
}

void impl::after_processing(downstream_msg::batch&) {
  processing_time_ += clock_type::now() - processing_begin_;
}

credit_controller::assignment impl::compute_initial() {
  return {buffer_size_, batch_size_};
}

credit_controller::assignment impl::compute(timespan) {
  // Update batch and buffer size if we have at least 10 new data points to
  // work with.
  if (num_elements_ > 9) {
    // Helper for truncating a 64-bit integer to a 32-bit integer with a
    // minimum value of 1.
    auto clamp_i32 = [](int64_t x) -> int32_t {
      static constexpr auto upper_bound = std::numeric_limits<int32_t>::max();
      if (x > upper_bound)
        return upper_bound;
      if (x <= 0)
        return 1;
      return static_cast<int32_t>(x);
    };
    // Calculate ideal batch size by complexity.
    auto total_ns = processing_time_.count();
    CAF_ASSERT(total_ns > 0);
    auto size_by_complexity = clamp_i32((complexity_.count() * num_elements_)
                                        / total_ns);
    // Calculate ideal batch size by size.
    auto bytes_per_element = clamp_i32(total_size_ / num_elements_);
    auto size_by_bytes = clamp_i32(bytes_per_batch_ / bytes_per_element);
    // Always pick the smaller of the two in order to achieve a good tradeoff
    // between size and computational complexity
    auto batch_size = clamp_i32(std::min(size_by_bytes, size_by_complexity));
    auto buffer_size = std::min(clamp_i32(buffer_capacity_ / bytes_per_element),
                                4 * batch_size_);
    // Add a new entry to our ringbuffer and calculate batch and buffer sizes
    // based on the average recorded sizes.
    auto& kvp = ringbuf_[ringbuf_pos_];
    kvp.first = buffer_size;
    kvp.second = batch_size;
    ringbuf_pos_ = (ringbuf_pos_ + 1) % ringbuf_.size();
    if (ringbuf_size_ < ringbuf_.size())
      ++ringbuf_size_;
    using int32_pair = std::pair<int32_t, int32_t>;
    auto plus = [](int32_pair x, int32_pair y) {
      return int32_pair{x.first + y.first, x.second + y.second};
    };
    auto psum = std::accumulate(ringbuf_.begin(),
                                ringbuf_.begin() + ringbuf_size_,
                                int32_pair{0, 0}, plus);
    buffer_size_ = psum.first / ringbuf_size_;
    batch_size_ = psum.second / ringbuf_size_;
    // Reset bookkeeping state.
    num_elements_ = 0;
    total_size_ = 0;
    processing_time_ = timespan{0};
  }
  return {buffer_size_, batch_size_};
}

credit_controller::assignment impl::compute_bridge() {
  CAF_ASSERT(batch_size_ > 0);
  CAF_ASSERT(buffer_size_ > batch_size_);
  return {buffer_size_, batch_size_};
}

int32_t impl::low_threshold() const noexcept {
  return buffer_size_ / 4;
}

} // namespace detail
} // namespace caf
