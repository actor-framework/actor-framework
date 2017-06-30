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

#ifndef CAF_DOWNSTREAM_POLICY_HPP
#define CAF_DOWNSTREAM_POLICY_HPP

#include <cstddef>

#include "caf/fwd.hpp"
#include "caf/duration.hpp"
#include "caf/downstream_path.hpp"

namespace caf {

/// Type-erased policy for dispatching to downstream paths.
class downstream_policy {
public:
  // -- member types -----------------------------------------------------------

  /// A raw pointer to a downstream path.
  using path_ptr = downstream_path*;

  /// A unique pointer to a downstream path.
  using path_uptr = std::unique_ptr<downstream_path>;

  /// Stores all available paths.
  using path_uptr_list = std::vector<path_uptr>;

  /// List of views to paths.
  using path_ptr_list = std::vector<path_ptr>;

  // -- constructors, destructors, and assignment operators --------------------

  downstream_policy(local_actor* selfptr, const stream_id& id);

  virtual ~downstream_policy();

  // -- communication to downstream actors -------------------------------------

  /// Tries sending batches to downstream actors.
  /// @param hint Optionally passes the last result of `total_credit` to
  ///             avoid recalculation.
  virtual void emit_batches() = 0;

  // -- feedback to upstream policy --------------------------------------------

  /// Returns the currently available credit, depending on the policy in use.
  /// For example, a broadcast policy would return the minimum of all available
  /// downstream credits.
  virtual long credit() const = 0;

  // -- static utility functions for path_uptr_list and path_ptr_list access ---
  
  /// Sorts `xs` in descending order by available credit.
  template <class PathContainer>
  static void sort_by_credit(PathContainer& xs) {
    using value_type = typename PathContainer::value_type;
    auto cmp = [](const value_type& x, const value_type& y) {
      return x->open_credit > y->open_credit;
    };
    std::sort(xs.begin(), xs.end(), cmp);
  }

  template <class T, class PathContainer, class F>
  static T fold(PathContainer& xs, T init, F f) {
    auto b = xs.begin();
    auto e = xs.end();
    return b != e ? std::accumulate(b, e, init, f) : 0;
  }

  /// Returns the total available credit for all sinks in `xs` in O(n).
  template <class PathContainer>
  static long total_credit(const PathContainer& xs) {
    return fold(xs, 0u,
                [=](long x, const typename PathContainer::value_type& y) {
                  return x + y->open_credit;
                });
  }

  /// Returns the maximum credit of all sinks in `paths_` in O(n).
  template <class PathContainer>
  static long max_credit(const PathContainer& xs) {
    return fold(xs, 0,
                [](long x, const typename PathContainer::value_type& y) {
                  return std::max(x, y->open_credit);
                });
  }

  /// Returns the minimal credit of all sinks in `xs` in O(n).
  template <class PathContainer>
  static long min_credit(const PathContainer& xs) {
    return fold(xs, std::numeric_limits<long>::max(),
                [](long x, const typename PathContainer::value_type& y) {
                  return std::min(x, y->open_credit);
                });
  }

  template <class PathContainer>
  static downstream_path* find(PathContainer& xs, const strong_actor_ptr& ptr) {
    auto predicate = [&](const typename PathContainer::value_type& y) {
      return y->hdl == ptr;
    };
    auto e = xs.end();
    auto i = std::find_if(xs.begin(), e, predicate);
    if (i != e)
      return &(*(*i)); // Ugly, but works for both raw and smart pointers.
    return nullptr;
  }

  // -- credit observers -------------------------------------------------------

  /// Returns the sum of all credits of all sinks in `paths_` in O(n).
  long total_credit() const;

  /// Returns the maximum credit of all sinks in `paths_` in O(n).
  long max_credit() const;

  /// Returns the minimal net credit of all sinks in `paths_` in O(n).
  long min_credit() const;

  // -- type-erased access to the buffer ---------------------------------------

  /// Returns the size of the output buffer.
  virtual long buf_size() const = 0;

  // -- path management --------------------------------------------------------

  /// Adds a path with in-flight `stream_msg::open` message.
  bool add_path(strong_actor_ptr ptr);

  /// Confirms a path and properly initialize its state.
  bool confirm_path(const strong_actor_ptr& rebind_from, strong_actor_ptr& ptr,
                    bool is_redeployable);

  /// Removes a downstream path without aborting the stream.
  virtual bool remove_path(strong_actor_ptr& ptr);

  /// Returns the state for `ptr.
  downstream_path* find(const strong_actor_ptr& ptr) const;

  /// Removes all paths.
  void close();

  /// Sends an abort message to all downstream actors and closes the stream.
  void abort(strong_actor_ptr& cause, const error& reason);

  /// Returns `true` if no downstream exists, `false` otherwise.
  inline bool closed() const {
    return paths_.empty();
  }

  inline long num_paths() const {
    return static_cast<long>(paths_.size());
  }

  inline const path_uptr_list& paths() const {
    return paths_;
  }

  // -- required stream state --------------------------------------------------

  inline local_actor* self() const {
    return self_;
  }

  const stream_id& sid() const {
    return sid_;
  }

  // -- configuration parameters -----------------------------------------------

  /// Minimum amount of messages required to emit a batch. A value of 0
  /// disables batch delays.
  inline long min_batch_size() const {
    return min_batch_size_;
  }

  /// Maximum amount of messages to put into a single batch. Causes the actor
  /// to split a buffer into more batches if necessary.
  inline long max_batch_size() const {
    return max_batch_size_;
  }

  /// Minimum amount of messages we wish to store at the actor in order to emit
  /// new batches immediately when receiving new downstream demand.
  inline long min_buffer_size() const {
    return min_buffer_size_;
  }

  /// Forces to actor to emit a batch even if the minimum batch size was not
  /// reached.
  inline duration max_batch_delay() const {
    return max_batch_delay_;
  }

protected:
  // -- virtually dispatched implementation details ----------------------------

  /// Broadcasts the first `*hint` elements of the buffer on all paths. If
  /// `hint == nullptr` then `min_credit()` is used instead.
  virtual void emit_broadcast() = 0;

  /// Sends `*hint` elements of the buffer to available paths. If
  /// `hint == nullptr` then `total_credit()` is used instead.
  virtual void emit_anycast() = 0;

  // -- utility functions for derived classes ----------------------------------

  /// Sorts `paths_` according to the open downstream credit.
  void sort_paths_by_credit();

  /// Emits the type-erased batch `xs` to `dest`.
  void emit_batch(downstream_path& dest, size_t xs_size, message xs);

  // -- required stream state --------------------------------------------------

  local_actor* self_;

  stream_id sid_;

  // -- configuration parameters -----------------------------------------------

  long min_batch_size_;

  long max_batch_size_;

  long min_buffer_size_;

  duration max_batch_delay_;

  // -- path management --------------------------------------------------------

  path_uptr_list paths_;
};

} // namespace caf

#endif // CAF_DOWNSTREAM_POLICY_HPP
