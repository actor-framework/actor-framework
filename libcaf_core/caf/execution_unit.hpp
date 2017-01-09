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

#ifndef CAF_EXECUTION_UNIT_HPP
#define CAF_EXECUTION_UNIT_HPP

#include "caf/fwd.hpp"

#include "caf/config.hpp"

namespace caf {

/// Identifies an execution unit, e.g., a worker thread of the scheduler. By
/// querying its execution unit, an actor can access other context information.
class execution_unit {
public:
  explicit execution_unit(actor_system* sys);

  execution_unit(execution_unit&&) = delete;
  execution_unit(const execution_unit&) = delete;

  virtual ~execution_unit();

  /// Enqueues `ptr` to the job list of the execution unit.
  /// @warning Must only be called from a {@link resumable} currently
  ///          executed by this execution unit.
  virtual void exec_later(resumable* ptr, bool high_prio = true) = 0;

  /// Checks if `ptr` has a high memory locality to this execution_unit
  virtual bool is_neighbor(execution_unit* ptr) const = 0;

  /// Returns the enclosing actor system.
  /// @warning Must be set before the execution unit calls `resume` on an actor.
  actor_system& system() const {
    CAF_ASSERT(system_ != nullptr);
    return *system_;
  }

  /// Returns a pointer to the proxy factory currently associated to this unit.
  proxy_registry* proxy_registry_ptr() {
    return proxies_;
  }

  /// Associated a new proxy factory to this unit.
  void proxy_registry_ptr(proxy_registry* ptr) {
    proxies_ = ptr;
  }

protected:
  actor_system* system_;
  proxy_registry* proxies_;
};

} // namespace caf

#endif // CAF_EXECUTION_UNIT_HPP
