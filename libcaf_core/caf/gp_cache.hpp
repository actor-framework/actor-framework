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

#ifndef CAF_GP_CACHE_HPP
#define CAF_GP_CACHE_HPP

#include <unordered_map>

#include "caf/fwd.hpp"
#include "caf/locks.hpp"

#include "caf/detail/shared_spinlock.hpp"

namespace caf {

/// Generel purpose cache for shared types
template <class Key, class Value>
class gp_cache {
public:
  using map_type = std::unordered_map<Key, Value>;
  using exclusive_guard = unique_lock<detail::shared_spinlock>;
  using shared_guard = shared_lock<detail::shared_spinlock>;

  friend class actor_system;

  gp_cache() = default;

  gp_cache(const gp_cache&) = delete;

  ~gp_cache() {
    // nop
  }

  ///
  Value get(Key key) const {
    shared_guard guard{mtx_};
    auto i = map_.find(key);
    return i != map_.end() ? i->second : nullptr;
  }

  ///
  bool put(Key key, Value value) {
    exclusive_guard guard{mtx_};
    return map_.emplace(key, std::move(value)).second;
  }

  ///
  void erase(Key key) {
    exclusive_guard guard{mtx_};
    map_.erase(key);
  }

  ///
  const map_type& get_cache() const {
    exclusive_guard guard{mtx_};
    return map_;
  }

private:
  map_type map_;

  mutable detail::shared_spinlock mtx_;
};

} // namespace caf

#endif // CAF_GP_CACHE_HPP
