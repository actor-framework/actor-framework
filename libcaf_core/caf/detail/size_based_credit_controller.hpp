// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/credit_controller.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/serialized_size.hpp"
#include "caf/downstream_msg.hpp"
#include "caf/stream.hpp"

namespace caf::detail {

/// A credit controller that estimates the bytes required to store incoming
/// batches and constrains credit based on upper bounds for memory usage.
class CAF_CORE_EXPORT size_based_credit_controller : public credit_controller {
public:
  // -- constants --------------------------------------------------------------

  /// Configures how many samples we require for recalculating buffer sizes.
  static constexpr int32_t min_samples = 50;

  /// Stores how many elements we buffer at most after the handshake.
  int32_t initial_buffer_size = 10;

  /// Stores how many elements we allow per batch after the handshake.
  int32_t initial_batch_size = 2;

  // -- constructors, destructors, and assignment operators --------------------

  explicit size_based_credit_controller(local_actor* self);

  ~size_based_credit_controller() override;

  // -- interface functions ----------------------------------------------------

  calibration init() override;

  calibration calibrate() override;

  // -- factory functions ------------------------------------------------------

  template <class T>
  static auto make(local_actor* self, stream<T>) {
    class impl : public size_based_credit_controller {
      using size_based_credit_controller::size_based_credit_controller;

      void before_processing(downstream_msg::batch& x) override {
        if (++this->sample_counter_ == this->sampling_rate_) {
          this->sample_counter_ = 0;
          this->inspector_.result = 0;
          this->sampled_elements_ += x.xs_size;
          for (auto& element : x.xs.get_as<std::vector<T>>(0))
            detail::save(this->inspector_, element);
          this->sampled_total_size_
            += static_cast<int64_t>(this->inspector_.result);
        }
      }
    };
    return std::make_unique<impl>(self);
  }

protected:
  // -- member variables -------------------------------------------------------

  local_actor* self_;

  /// Keeps track of when to sample a batch.
  int32_t sample_counter_ = 0;

  /// Stores the last computed (moving) average for the serialized size per
  /// element in the stream.
  int32_t bytes_per_element_ = 0;

  /// Stores how many elements were sampled since last calling `calibrate`.
  int32_t sampled_elements_ = 0;

  /// Stores how many bytes the sampled batches required when serialized.
  int64_t sampled_total_size_ = 0;

  /// Computes how many bytes elements require on the wire.
  serialized_size_inspector inspector_;

  /// Stores whether this is the first run.
  bool initializing_ = true;

  // --  see caf::defaults::stream::size_policy --------------------------------

  int32_t bytes_per_batch_;

  int32_t buffer_capacity_;

  int32_t sampling_rate_ = 1;

  int32_t calibration_interval_;

  float smoothing_factor_;
};

} // namespace caf::detail
