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

namespace caf::detail {

/// A credit controller that estimates the bytes required to store incoming
/// batches and constrains credit based on upper bounds for memory usage.
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
  int64_t num_batches_ = 0;

  /// Stores how many elements the buffer should hold at most.
  int32_t buffer_size_ = 10;

  /// Stores how many elements each batch should contain.
  int32_t batch_size_ = 2;

  /// Configures how many bytes we store in total.
  int32_t buffer_capacity_;

  /// Configures how many bytes we transfer per batch.
  int32_t bytes_per_batch_;

  /// Stores how many elements we have sampled during the current cycle.
  int64_t sampled_elements_ = 0;

  /// Stores approximately how many bytes the sampled elements require when
  /// serialized.
  int64_t sampled_total_size_ = 0;

  /// Counter for keeping track of when to sample a batch.
  int32_t sample_counter_ = 0;

  /// Configured how many batches we skip for the size sampling.
  int32_t sample_rate_ = 50;
};

} // namespace caf::detail
