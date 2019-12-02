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

namespace caf::detail {

impl::impl(scheduled_actor* self) : super(self) {
  auto& cfg = self->system().config();
  buffer_capacity_ = get_or(cfg, "stream.size-policy.buffer-capacity",
                            defaults::stream::size_policy::buffer_capacity);
  bytes_per_batch_ = get_or(cfg, "stream.size-policy.bytes-per-batch",
                            defaults::stream::size_policy::bytes_per_batch);
}

impl::~impl() {
  // nop
}

void impl::before_processing(downstream_msg::batch& x) {
  if (++sample_counter_ == sample_rate_) {
    sampled_elements_ += x.xs_size;
    sampled_total_size_ += serialized_size(self()->system(), x.xs);
    sample_counter_ = 0;
  }
  ++num_batches_;
}

void impl::after_processing(downstream_msg::batch&) {
  // nop
}

credit_controller::assignment impl::compute_initial() {
  return {buffer_size_, batch_size_};
}

credit_controller::assignment impl::compute(timespan, int32_t) {
  if (sampled_elements_ >= min_samples) {
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
    // Calculate ideal batch size by size.
    auto bytes_per_element = clamp_i32(sampled_total_size_ / sampled_elements_);
    batch_size_ = clamp_i32(bytes_per_batch_ / bytes_per_element);
    buffer_size_ = clamp_i32(buffer_capacity_ / bytes_per_element);
    // Reset bookkeeping state.
    sampled_elements_ = 0;
    sampled_total_size_ = 0;
    // Adjust the sample rate to reach min_samples in the next cycle.
    sample_rate_ = clamp_i32(num_batches_ / min_samples);
    if (sample_counter_ >= sample_rate_)
      sample_counter_ = 0;
    num_batches_ = 0;
  }
  return {buffer_size_, batch_size_};
}

credit_controller::assignment impl::compute_bridge() {
  CAF_ASSERT(batch_size_ > 0);
  CAF_ASSERT(buffer_size_ > batch_size_);
  return {buffer_size_, batch_size_};
}

int32_t impl::threshold() const noexcept {
  return static_cast<int32_t>(buffer_size_ * buffer_threshold);
}

} // namespace caf::detail
