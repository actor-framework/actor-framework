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

#ifndef CAF_ABSTRACT_DOWNSTREAM_HPP
#define CAF_ABSTRACT_DOWNSTREAM_HPP

#include <set>
#include <vector>
#include <memory>
#include <numeric>
#include <unordered_map>

#include "caf/fwd.hpp"
#include "caf/atom.hpp"
#include "caf/optional.hpp"
#include "caf/stream_id.hpp"

namespace caf {

class abstract_downstream {
public:
  using topics = std::vector<atom_value>;

  using path = downstream_path;

  using path_cref = const path&;

  /// A unique pointer to a downstream path.
  using path_uptr = std::unique_ptr<path>;

  /// A raw pointer to a downstream path.
  using path_ptr = path*;

  /// Stores all available paths.
  using path_list = std::vector<path_uptr>;

  /// List of views to paths.
  using path_ptr_list = std::vector<path_ptr>;

  /// Stores all available paths sorted by topics.
  using path_map = std::unordered_map<topics, path_ptr_list>;

  using policy_ptr = std::unique_ptr<downstream_policy>;

  abstract_downstream(local_actor* selfptr, const stream_id& sid,
                      policy_ptr ptr);

  virtual ~abstract_downstream();

  /// Returns the total available credit for all sinks in `xs` in O(n).
  template <class PathContainer>
  static long total_credit(const PathContainer& xs) {
    return fold(xs, 0u,
                [=](long x, const typename PathContainer::value_type& y) {
                  return x + y->open_credit;
                });
  }

  /// Returns the total available credit for all sinks in `paths_` in O(n).
  long total_credit() const;

  /// Returns the maximum credit of all sinks in `paths_` in O(n).
  template <class PathContainer>
  static long max_credit(const PathContainer& xs) {
    return fold(xs, 0,
                [](long x, const typename PathContainer::value_type& y) {
                  return std::max(x, y->open_credit);
                });
  }

  /// Returns the maximum credit of all sinks in `paths_` in O(n).
  long max_credit() const;

  /// Returns the minimal credit of all sinks in `xs` in O(n).
  template <class PathContainer>
  static long min_credit(const PathContainer& xs) {
    return fold(xs, std::numeric_limits<long>::max(),
                [](long x, const typename PathContainer::value_type& y) {
                  return std::min(x, y->open_credit);
                });
  }

  /// Returns the minimal net credit of all sinks in `paths_` in O(n).
  long min_credit() const;

  /// Broadcasts the first `*hint` elements of the buffer on all paths. If
  /// `hint == nullptr` then `min_credit()` is used instead.
  virtual void broadcast(long* hint = nullptr) = 0;

  /// Sends `*hint` elements of the buffer to available paths. If
  /// `hint == nullptr` then `total_credit()` is used instead.
  virtual void anycast(long* hint = nullptr) = 0;

  /// Returns the size of the output buffer.
  virtual long buf_size() const = 0;

  /// Adds a path with in-flight `stream_msg::open` message.
  bool add_path(strong_actor_ptr ptr);

  /// Confirms a path and properly initialize its state.
  bool confirm_path(const strong_actor_ptr& rebind_from, strong_actor_ptr& ptr,
                    bool is_redeployable);

  /// Removes a downstream path without aborting the stream.
  bool remove_path(strong_actor_ptr& ptr);

  /// Removes all paths.
  void close();

  /// Sends an abort message to all paths and closes the stream.
  void abort(strong_actor_ptr& cause, const error& reason);

  template <class PathContainer>
  static path* find(PathContainer& xs, const strong_actor_ptr& ptr) {
    auto predicate = [&](const typename PathContainer::value_type& y) {
      return y->hdl == ptr;
    };
    auto e = xs.end();
    auto i = std::find_if(xs.begin(), e, predicate);
    if (i != e)
      return &(*(*i)); // Ugly, but works for both raw and smart pointers.
    return nullptr;
  }

  path* find(const strong_actor_ptr& ptr) const;

  long total_net_credit() const;

  virtual long num_paths() const;

  inline local_actor* self() const {
    return self_;
  }

  inline downstream_policy& policy() const {
    return *policy_;
  }

  inline const stream_id& sid() const {
    return sid_;
  }

  inline bool closed() const {
    return paths_.empty();
  }

  /// Returns how many items should be stored on individual paths in order to
  /// minimize latency between received demand and sent batches.
  inline long min_buffer_size() const {
    return min_buffer_size_;
  }

  /// Returns all currently available paths on this downstream.
  const path_list& paths() const {
    return paths_;
  }

protected:
  void send_batch(downstream_path& dest, long chunk_size, message chunk);

  /// Sorts `xs` in descending order by available credit.
  template <class PathContainer>
  static void sort_by_credit(PathContainer& xs) {
    using value_type = typename PathContainer::value_type;
    auto cmp = [](const value_type& x, const value_type& y) {
      return x->open_credit > y->open_credit;
    };
    std::sort(xs.begin(), xs.end(), cmp);
  }

  /// Sorts `paths_` in descending order by available credit.
  void sort_by_credit();

  template <class T, class PathContainer, class F>
  static T fold(PathContainer& xs, T init, F f) {
    auto b = xs.begin();
    auto e = xs.end();
    return b != e ? std::accumulate(b, e, init, f) : 0;
  }

  local_actor* self_;
  stream_id sid_;
  path_list paths_;
  long min_buffer_size_;
  std::unique_ptr<downstream_policy> policy_;
};

} // namespace caf

#endif // CAF_ABSTRACT_DOWNSTREAM_HPP
