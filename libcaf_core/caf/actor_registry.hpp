/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "caf/abstract_actor.hpp"
#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/shared_spinlock.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// A registry is used to associate actors to IDs or names. This allows a
/// middleman to lookup actor handles after receiving actor IDs via the network
/// and enables developers to use well-known names to identify important actors
/// independent from their ID at runtime. Note that the registry does *not*
/// contain all actors of an actor system. The middleman registers actors as
/// needed.
class CAF_CORE_EXPORT actor_registry {
public:
  friend class actor_system;

  ~actor_registry();

  /// Returns the local actor associated to `key`.
  template <class T = strong_actor_ptr>
  T get(actor_id key) const {
    return actor_cast<T>(get_impl(key));
  }

  /// Associates a local actor with its ID.
  template <class T>
  void put(actor_id key, const T& val) {
    put_impl(key, actor_cast<strong_actor_ptr>(val));
  }

  /// Removes an actor from this registry,
  /// leaving `reason` for future reference.
  void erase(actor_id key);

  /// Increases running-actors-count by one.
  void inc_running();

  /// Decreases running-actors-count by one.
  void dec_running();

  /// Returns the number of currently running actors.
  size_t running() const;

  /// Blocks the caller until running-actors-count becomes `expected`
  /// (must be either 0 or 1).
  void await_running_count_equal(size_t expected) const;

  /// Returns the actor associated with `key` or `invalid_actor`.
  template <class T = strong_actor_ptr>
  T get(const std::string& key) const {
    return actor_cast<T>(get_impl(key));
  }

  /// Associates given actor to `key`.
  template <class T>
  void put(const std::string& key, const T& value) {
    // using reference here and before to allow putting a scoped_actor without
    // calling .ptr()
    put_impl(std::move(key), actor_cast<strong_actor_ptr>(value));
  }

  /// Removes a name mapping.
  void erase(const std::string& key);

  using name_map = std::unordered_map<std::string, strong_actor_ptr>;

  name_map named_actors() const;

private:
  // Starts this component.
  void start();

  // Stops this component.
  void stop();

  /// Returns the local actor associated to `key`.
  strong_actor_ptr get_impl(actor_id key) const;

  /// Associates a local actor with its ID.
  void put_impl(actor_id key, strong_actor_ptr val);

  /// Returns the actor associated with `key` or `invalid_actor`.
  strong_actor_ptr get_impl(const std::string& key) const;

  /// Associates given actor to `key`.
  void put_impl(const std::string& key, strong_actor_ptr value);

  using entries = std::unordered_map<actor_id, strong_actor_ptr>;

  actor_registry(actor_system& sys);

  std::atomic<size_t> running_;
  mutable std::mutex running_mtx_;
  mutable std::condition_variable running_cv_;

  mutable detail::shared_spinlock instances_mtx_;
  entries entries_;

  name_map named_entries_;
  mutable detail::shared_spinlock named_entries_mtx_;

  actor_system& system_;
};

} // namespace caf
