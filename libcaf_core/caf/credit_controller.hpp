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

#include "caf/downstream_msg.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// Computes credit for an attached source.
class credit_controller {
public:
  // -- member types -----------------------------------------------------------

  /// Wraps an assignment of the controller to its source.
  struct assignment {
    /// Store how much credit we assign to the source.
    int32_t credit;

    /// Store how many elements we demand per batch.
    int32_t batch_size;
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit credit_controller(scheduled_actor* self);

  virtual ~credit_controller();

  // -- properties -------------------------------------------------------------

  scheduled_actor* self() noexcept {
    return self_;
  }

  // -- pure virtual functions -------------------------------------------------

  /// Called before processing the batch `x` in order to allow the controller
  /// to keep statistics on incoming batches.
  virtual void before_processing(downstream_msg::batch& x) = 0;

  /// Called after processing the batch `x` in order to allow the controller to
  /// keep statistics on incoming batches.
  /// @note The consumer may alter the batch while processing it, for example
  ///       by moving each element of the batch downstream.
  virtual void after_processing(downstream_msg::batch& x) = 0;

  /// Assigns initial credit during the stream handshake.
  /// @returns The initial credit for the new sources.
  virtual assignment compute_initial() = 0;

  /// Computes a credit assignment to the source after a cycle ends.
  virtual assignment compute(timespan cycle) = 0;

  // -- virtual functions ------------------------------------------------------

  /// Computes a credit assignment to the source after crossing the
  /// low-threshold. May assign zero credit.
  virtual assignment compute_bridge();

  /// Returns the threshold for when we may give extra credit to a source
  /// during a cycle.
  /// @returns A negative value if the controller never grants bridge credit,
  ///          otherwise the threshold for calling `compute_bridge` to generate
  ///          additional credit.
  virtual int32_t low_threshold() const noexcept;

private:
  // -- member variables -------------------------------------------------------

  /// Points to the parent system.
  scheduled_actor* self_;
};

} // namespace caf
