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

namespace caf::detail {

/// A credit controller that estimates the bytes required to store incoming
/// batches and constrains credit based on upper bounds for memory usage.
class token_based_credit_controller : public credit_controller {
public:
  // -- constants --------------------------------------------------------------

  /// Configures how many samples we require for recalculating buffer sizes.
  static constexpr int32_t min_samples = 50;

  /// Stores how many elements we buffer at most after the handshake.
  int32_t initial_buffer_size = 10;

  /// Stores how many elements we allow per batch after the handshake.
  int32_t initial_batch_size = 2;

  // -- constructors, destructors, and assignment operators --------------------

  explicit token_based_credit_controller(local_actor* self);

  ~token_based_credit_controller() override;

  // -- interface functions ----------------------------------------------------

  void before_processing(downstream_msg::batch& batch) override;

  calibration init() override;

  calibration calibrate() override;

private:
  // --  see caf::defaults::stream::token_policy -------------------------------

  int32_t batch_size_;

  int32_t buffer_size_;
};

} // namespace caf::detail
