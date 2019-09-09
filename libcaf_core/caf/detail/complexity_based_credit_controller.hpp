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

namespace caf {
namespace detail {

/// Computes credit for an attached source based on measuring the complexity of
/// incoming batches.
class complexity_based_credit_controller : public credit_controller {
public:
  // -- member types -----------------------------------------------------------

  using super = credit_controller;

  // -- constructors, destructors, and assignment operators --------------------

  explicit complexity_based_credit_controller(scheduled_actor* self);

  ~complexity_based_credit_controller() override;

  // -- implementation of virtual functions ------------------------------------

  void before_processing(downstream_msg::batch& x) override;

  void after_processing(downstream_msg::batch& x) override;

  assignment compute_initial() override;

  assignment compute(timespan cycle) override;

private:
  // -- member variables -------------------------------------------------------

  /// Total number of elements in all processed batches in the current cycle.
  int64_t num_elements_ = 0;

  /// Elapsed time for processing all elements of all batches in the current
  /// cycle.
  timespan processing_time_;

  /// Timestamp of the last call to `before_processing`.
  timestamp processing_begin_;

  /// Stores the desired per-batch complexity.
  timespan complexity_;
};

} // namespace detail
} // namespace caf
