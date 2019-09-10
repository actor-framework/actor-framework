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

#pragma once

#include "caf/credit_controller.hpp"

#include <chrono>

namespace caf {
namespace detail {

/// .
class size_based_credit_controller : public credit_controller {
public:
  // -- member types -----------------------------------------------------------

  using super = credit_controller;

  using clock_type = std::chrono::steady_clock;

  using time_point = clock_type::time_point;

  // -- constructors, destructors, and assignment operators --------------------

  explicit size_based_credit_controller(scheduled_actor* self);

  ~size_based_credit_controller() override;

  // -- implementation of virtual functions ------------------------------------

  void before_processing(downstream_msg::batch& x) override;

  void after_processing(downstream_msg::batch& x) override;

  assignment compute_initial() override;

  assignment compute(timespan cycle) override;

  assignment compute_bridge() override;

  int32_t low_threshold() const noexcept override;

private:
  // -- member variables -------------------------------------------------------

  /// Total number of elements in all processed batches in the current cycle.
  int64_t num_elements_ = 0;

  /// Measured size of all sampled elements.
  int64_t total_size_ = 0;

  /// Elapsed time for processing all elements of all batches in the current
  /// cycle.
  timespan processing_time_;

  /// Timestamp of the last call to `before_processing`.
  time_point processing_begin_;

  /// Stores the desired per-batch complexity.
  timespan complexity_;

  /// Stores how many elements the buffer should hold at most.
  int32_t buffer_size_ = 1;

  /// Stores how many elements each batch should contain.
  int32_t batch_size_ = 1;

  /// Configures how many bytes we store in total.
  int32_t buffer_capacity_;

  /// Configures how many bytes we transfer per batch.
  int32_t bytes_per_batch_;

  /// Stores the current write position in the ring buffer.
  size_t ringbuf_pos_ = 0;

  /// Stores how many valid entries the ring buffer holds.
  size_t ringbuf_size_ = 0;

  /// Records recent calculations for buffer and batch sizes.
  std::array<std::pair<int32_t, int32_t>, 32> ringbuf_;
};

} // namespace detail
} // namespace caf
