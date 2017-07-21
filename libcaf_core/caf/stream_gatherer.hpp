/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_STREAM_GATHERER_HPP
#define CAF_STREAM_GATHERER_HPP

#include <vector>
#include <cstdint>
#include <utility>

#include "caf/fwd.hpp"

namespace caf {

/// Type-erased policy for receiving data from sources.
class stream_gatherer {
public:
  // -- member types -----------------------------------------------------------

  /// Type of a single path to a data source.
  using path_type = inbound_path;

  /// Pointer to a single path to a data source.
  using path_ptr = path_type*;

  // -- constructors, destructors, and assignment operators --------------------

  stream_gatherer() = default;

  virtual ~stream_gatherer();

  // -- pure virtual memeber functions -----------------------------------------

  /// Adds a path to the edge and emits `ack_open` to the source.
  /// @param sid Stream ID used by the source.
  /// @param x Handle to the source.
  /// @param original_stage Actor that received the stream handshake initially.
  /// @param prio Priority of data on this path.
  /// @param available_credit Maximum credit for granting to the source.
  /// @param redeployable Stores whether the source can re-appear after aborts.
  /// @param result_cb Callback for the listener of the final stream result.
  ///                  The gatherer must ignore the promise when returning
  ///                  `nullptr`, because the previous stage is responsible for
  ///                  it until the gatherer acknowledges the handshake. The
  ///                  callback is invalid if the stream has a next stage.
  /// @returns The added path on success, `nullptr` otherwise.
  virtual path_ptr add_path(const stream_id& sid, strong_actor_ptr x,
                            strong_actor_ptr original_stage,
                            stream_priority prio, long available_credit,
                            bool redeployable, response_promise result_cb) = 0;

  /// Removes a path from the gatherer.
  virtual bool remove_path(const stream_id& sid, const actor_addr& x,
                           error reason, bool silent) = 0;

  /// Removes all paths gracefully.
  virtual void close(message result) = 0;

  /// Removes all paths with an error message.
  virtual void abort(error reason) = 0;

  /// Returns the number of paths managed on this edge.
  virtual long num_paths() const = 0;

  /// Returns `true` if no downstream exists, `false` otherwise.
  virtual bool closed() const = 0;

  /// Returns whether this edge remains open after the last path is removed.
  virtual bool continuous() const = 0;

  /// Sets whether this edge remains open after the last path is removed.
  virtual void continuous(bool value) = 0;

  /// Returns the stored state for `x` if `x` is a known path and associated to
  /// `sid`, otherwise `nullptr`.
  virtual path_ptr find(const stream_id& sid, const actor_addr& x) = 0;

  /// Returns the stored state for `x` if `x` is a known path and associated to
  /// `sid`, otherwise `nullptr`.
  virtual path_ptr path_at(size_t index) = 0;

  /// Returns the point at which an actor stops sending out demand immediately
  /// (waiting for the available credit to first drop below the watermark).
  virtual long high_watermark() const = 0;

  /// Returns the minimum amount of credit required to send a `demand` message.
  virtual long min_credit_assignment() const = 0;

  /// Returns the maximum credit assigned to a single upstream actors.
  virtual long max_credit() const = 0;

  /// Sets the point at which an actor stops sending out demand immediately
  /// (waiting for the available credit to first drop below the watermark).
  virtual void high_watermark(long x) = 0;

  /// Sets the minimum amount of credit required to send a `demand` message.
  virtual void min_credit_assignment(long x) = 0;

  /// Sets the maximum credit assigned to a single upstream actors.
  virtual void max_credit(long x) = 0;

  /// Assigns new credit to all sources.
  virtual void assign_credit(long downstream_capacity) = 0;

  /// Calculates initial credit for `x` after adding it to the gatherer.
  virtual long initial_credit(long downstream_capacity, path_ptr x) = 0;

  // -- convenience functions --------------------------------------------------

  /// Removes a path from the gatherer.
  bool remove_path(const stream_id& sid, const strong_actor_ptr& x,
                   error reason, bool silent);

  /// Convenience function for calling `find(x, actor_cast<actor_addr>(x))`.
  path_ptr find(const stream_id& sid, const strong_actor_ptr& x);
};

} // namespace caf

#endif // CAF_STREAM_GATHERER_HPP
