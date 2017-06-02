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

#ifndef CAF_UPSTREAM_POLICY_HPP
#define CAF_UPSTREAM_POLICY_HPP

#include <vector>
#include <cstdint>
#include <utility>

#include "caf/fwd.hpp"

namespace caf {

class upstream_policy {
public:
  // -- member types -----------------------------------------------------------

  /// A raw pointer to a downstream path.
  using path_ptr = upstream_path*;

  /// A unique pointer to a upstream path.
  using path_uptr = std::unique_ptr<upstream_path>;

  /// Stores all available paths.
  using path_uptr_list = std::vector<path_uptr>;

  /// List of views to paths.
  using path_ptr_list = std::vector<path_ptr>;

  /// Describes an assignment of credit to an upstream actor.
  using assignment_pair = std::pair<upstream_path*, long>;

  /// Describes an assignment of credit to all upstream actors.
  using assignment_vec = std::vector<assignment_pair>;

  // -- constructors, destructors, and assignment operators --------------------

  upstream_policy(local_actor* selfptr);

  virtual ~upstream_policy();

  // -- path management --------------------------------------------------------

  /// Returns `true` if all upstream paths are closed and this upstream is not
  /// flagged as `continuous`, `false` otherwise.
  inline bool closed() const {
    return paths_.empty() && !continuous_;
  }

  /// Returns whether this upstream remains open even if no more upstream path
  /// exists.
  inline bool continuous() const {
    return continuous_;
  }

  /// Sets whether this upstream remains open even if no more upstream path
  /// exists.
  inline void continuous(bool value) {
    continuous_ = value;
  }

  /// Sends an abort message to all upstream actors and closes the stream.
  void abort(strong_actor_ptr& cause, const error& reason);


  /// Assigns credit to upstream actors according to the current capacity of
  /// all downstream actors (and a minimum buffer size) combined.
  void assign_credit(long downstream_capacity);

  /// Adds a new upstream actor and returns the initial credit.
  expected<long> add_path(strong_actor_ptr hdl, const stream_id& sid,
                          stream_priority prio,
                          long downstream_credit);

  bool remove_path(const strong_actor_ptr& hdl);

  upstream_path* find(const strong_actor_ptr& x) const;

  // -- required state ---------------------------------------------------------

  inline local_actor* self() const {
    return self_;
  }

  // -- configuration parameters -----------------------------------------------

  /// Returns the point at which an actor stops sending out demand immediately
  /// (waiting for the available credit to first drop below the watermark).
  long high_watermark() const {
    return high_watermark_;
  }

  /// Sets the point at which an actor stops sending out demand immediately
  /// (waiting for the available credit to first drop below the watermark).
  void high_watermark(long x) {
    high_watermark_ = x;
  }

  /// Returns the minimum amount of credit required to send a `demand` message.
  long min_credit_assignment() const {
    return min_credit_assignment_;
  }

  /// Sets the minimum amount of credit required to send a `demand` message.
  void min_credit_assignment(long x) {
    min_credit_assignment_ = x;
  }

  /// Returns the maximum credit assigned to a single upstream actors.
  long max_credit() const {
    return max_credit_;
  }

  /// Sets the maximum credit assigned to a single upstream actors.
  void max_credit(long x) {
    max_credit_ = x;
  }

protected:
  /// Assigns new credit to upstream actors by filling `assignment_vec_`.
  virtual void fill_assignment_vec(long downstream_credit) = 0;

  /// Pointer to the parent actor.
  local_actor* self_;

  /// List of all known paths.
  path_uptr_list paths_;

  /// An assignment vector that's re-used whenever calling the policy.
  assignment_vec assignment_vec_;

  /// Stores whether this stream remains open even if all paths have been
  /// closed.
  bool continuous_;

  long high_watermark_;
  long min_credit_assignment_;
  long max_credit_;
};

} // namespace caf

#endif // CAF_UPSTREAM_POLICY_HPP
