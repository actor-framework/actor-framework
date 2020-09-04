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

#include <cstdint>

#include "caf/detail/core_export.hpp"
#include "caf/downstream_msg.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// Computes credit for an attached source.
class CAF_CORE_EXPORT credit_controller {
public:
  // -- member types -----------------------------------------------------------

  /// Wraps an assignment of the controller to its source.
  struct calibration {
    /// Stores how much credit the path may emit at most.
    int32_t max_credit;

    /// Stores how many elements we demand per batch.
    int32_t batch_size;

    /// Stores how many batches the caller should wait before calling
    /// `calibrate` again.
    int32_t next_calibration;
  };

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~credit_controller();

  // -- pure virtual functions -------------------------------------------------

  /// Called before processing the batch `x` in order to allow the controller
  /// to keep statistics on incoming batches.
  virtual void before_processing(downstream_msg::batch& batch) = 0;

  /// Returns an initial calibration for the path.
  virtual calibration init() = 0;

  /// Computes a credit assignment to the source after crossing the
  /// low-threshold. May assign zero credit.
  virtual calibration calibrate() = 0;
};

} // namespace caf
