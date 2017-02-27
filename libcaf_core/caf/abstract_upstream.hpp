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

#ifndef CAF_ABSTRACT_UPSTREAM_HPP
#define CAF_ABSTRACT_UPSTREAM_HPP

#include <vector>
#include <memory>
#include <unordered_map>

#include "caf/fwd.hpp"
#include "caf/upstream_path.hpp"
#include "caf/upstream_policy.hpp"

namespace caf {

class abstract_upstream {
public:
  using path = upstream_path;

  using path_cref = const path&;

  using path_uptr = std::unique_ptr<path>;

  using path_ptr = path*;

  using path_list = std::vector<path_uptr>;

  using path_ptr_list = std::vector<path_ptr>;

  /// Stores available paths sorted by priority.
  using path_map = std::unordered_map<stream_priority, path_ptr_list>;

  using policy_ptr = std::unique_ptr<upstream_policy>;

  /// @pre `self != nullptr`
  /// @pre `policy != nullptr`
  abstract_upstream(local_actor* selfptr, policy_ptr p);

  virtual ~abstract_upstream();

  void abort(strong_actor_ptr& cause, const error& reason);

  error pull(strong_actor_ptr& hdl, size_t n);

  error pull(size_t n);

  expected<size_t> add_path(strong_actor_ptr hdl, const stream_id& sid,
                            stream_priority prio);

  bool remove_path(const strong_actor_ptr& hdl);

  inline local_actor* self() const {
    return self_;
  }

  /// Queries whether all upstream paths were closed.
  inline bool closed() const {
    return paths_.empty();
  }

protected:
  local_actor* self_;
  path_list paths_;
  policy_ptr policy_;
};

} // namespace caf

#endif // CAF_ABSTRACT_UPSTREAM_HPP
