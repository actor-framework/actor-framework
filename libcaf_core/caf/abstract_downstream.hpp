/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

  /// Returns the total available credit for all sinks in O(n).
  size_t total_credit() const;

  /// Returns the maximum credit of all sinks in O(n).
  size_t max_credit() const;

  /// Returns the minimal credit of all sinks in O(n).
  size_t min_credit() const;

  /// Broadcasts the first `*hint` elements of the buffer on all paths. If
  /// `hint == nullptr` then `min_credit()` is used instead.
  virtual void broadcast(size_t* hint = nullptr) = 0;

  /// Sends `*hint` elements of the buffer to available paths. If
  /// `hint == nullptr` then `total_credit()` is used instead.
  virtual void anycast(size_t* hint = nullptr) = 0;

  /// Returns the size of the output buffer.
  virtual size_t buf_size() const = 0;

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

  optional<path&> find(const strong_actor_ptr& ptr) const;

  size_t available_credit() const;

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

protected:
  void send_batch(downstream_path& dest, size_t chunk_size, message chunk);

  /// Sorts `paths_` in descending order by available credit.
  void sort_by_credit();

  template <class F>
  size_t fold_paths(size_t init, F f) const {
    auto b = paths_.begin();
    auto e = paths_.end();
    auto g = [&](size_t x, const path_uptr& y) {
      return f(x, *y);
    };
    return b != e ? std::accumulate(b, e, init, g) : 0;
  }

  local_actor* self_;
  stream_id sid_;
  path_list paths_;
  std::unique_ptr<downstream_policy> policy_;
};

} // namespace caf

#endif // CAF_ABSTRACT_DOWNSTREAM_HPP
