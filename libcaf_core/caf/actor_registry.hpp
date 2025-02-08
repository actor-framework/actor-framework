// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <string>
#include <unordered_map>

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

  virtual ~actor_registry();

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
  virtual void erase(actor_id key) = 0;

  /// Increases running-actors-count by one.
  /// @returns the increased count.
  virtual size_t inc_running() = 0;

  /// Decreases running-actors-count by one.
  /// @returns the decreased count.
  virtual size_t dec_running() = 0;

  /// Returns the number of currently running actors.
  virtual size_t running() const = 0;

  /// Blocks the caller until running-actors-count becomes `expected`
  /// (must be either 0 or 1).
  virtual void await_running_count_equal(size_t expected) const = 0;

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
  virtual void erase(const std::string& key) = 0;

  using name_map = std::unordered_map<std::string, strong_actor_ptr>;

  virtual name_map named_actors() const = 0;

private:
  /// Returns the local actor associated to `key`.
  virtual strong_actor_ptr get_impl(actor_id key) const = 0;

  /// Associates a local actor with its ID.
  virtual void put_impl(actor_id key, strong_actor_ptr val) = 0;

  /// Returns the actor associated with `key` or `invalid_actor`.
  virtual strong_actor_ptr get_impl(const std::string& key) const = 0;

  /// Associates given actor to `key`.
  virtual void put_impl(const std::string& key, strong_actor_ptr value) = 0;
};

} // namespace caf
