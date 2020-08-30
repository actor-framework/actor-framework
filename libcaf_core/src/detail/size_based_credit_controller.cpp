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

namespace {

// Sample the first 10 batches when starting up.
constexpr int32_t initial_sample_size = 10;

} // namespace

namespace caf::detail {

size_based_credit_controller::size_based_credit_controller(local_actor* ptr)
  : self_(ptr) {
  namespace fallback = defaults::stream::size_policy;
  // Initialize from the config parameters.
  auto& cfg = ptr->system().config();
  if (auto section = get_if<settings>(&cfg, "caf.stream.size-based-policy")) {
    bytes_per_batch_ = get_or(*section, "bytes-per-batch",
                              fallback::bytes_per_batch);
    buffer_capacity_ = get_or(*section, "buffer-capacity",
                              fallback::buffer_capacity);
    calibration_interval_ = get_or(*section, "calibration-interval",
                                   fallback::calibration_interval);
    smoothing_factor_ = get_or(*section, "smoothing-factor",
                               fallback::smoothing_factor);
  } else {
    buffer_capacity_ = fallback::buffer_capacity;
    bytes_per_batch_ = fallback::bytes_per_batch;
    calibration_interval_ = fallback::calibration_interval;
    smoothing_factor_ = fallback::smoothing_factor;
  }
}

size_based_credit_controller::~size_based_credit_controller() {
  // nop
}

void size_based_credit_controller::before_processing(downstream_msg::batch& x) {
  if (++sample_counter_ == sampling_rate_) {
    sample_counter_ = 0;
    sampled_elements_ += x.xs_size;
    // FIXME: this wildly overestimates the actual size per element, because we
    //        include the size of the meta data per message. This also punishes
    //        small batches and we probably never reach a stable point even if
    //        incoming data always has the exact same size per element.
    sampled_total_size_ += static_cast<int64_t>(serialized_size(x.xs));
  }
}

credit_controller::calibration size_based_credit_controller::init() {
  // Initially, we simply assume that the size of one element equals
  // bytes-per-batch.
  return {buffer_capacity_ / bytes_per_batch_, 1, initial_sample_size};
}

credit_controller::calibration size_based_credit_controller::calibrate() {
  CAF_ASSERT(sample_counter_ == 0);
  // Helper for truncating a 64-bit integer to a 32-bit integer with a minimum
  // value of 1.
  auto clamp_i32 = [](int64_t x) -> int32_t {
    static constexpr auto upper_bound = std::numeric_limits<int32_t>::max();
    if (x > upper_bound)
      return upper_bound;
    if (x <= 0)
      return 1;
    return static_cast<int32_t>(x);
  };
  if (!initializing_) {
    auto bpe = clamp_i32(sampled_total_size_ / calibration_interval_);
    bytes_per_element_ = static_cast<int32_t>(
      smoothing_factor_ * bpe // weighted current measurement
      + (1.0 - smoothing_factor_) * bytes_per_element_ // past values
    );
  } else {
    // After our first run, we continue with the actual sampling rate.
    initializing_ = false;
    sampling_rate_ = get_or(self_->config(),
                            "caf.stream.size-policy.sample-rate",
                            defaults::stream::size_policy::sampling_rate);
    bytes_per_element_ = clamp_i32(sampled_total_size_ / initial_sample_size);
  }
  sampled_total_size_ = 0;
  return {
    clamp_i32(buffer_capacity_ / bytes_per_element_),
    clamp_i32(bytes_per_batch_ / bytes_per_element_),
    sampling_rate_ * calibration_interval_,
  };
}

} // namespace caf::detail
