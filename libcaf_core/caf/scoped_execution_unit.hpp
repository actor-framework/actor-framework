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

#ifndef CAF_SCOPED_EXECUTION_UNIT_HPP
#define CAF_SCOPED_EXECUTION_UNIT_HPP

#include "caf/execution_unit.hpp"

namespace caf {

/// Identifies an execution unit, e.g., a worker thread of the scheduler. By
/// querying its execution unit, an actor can access other context information.
class scoped_execution_unit : public execution_unit {
public:
  /// @pre `sys != nullptr`
  explicit scoped_execution_unit(actor_system* sys = nullptr);

  /// Delegates the resumable to the scheduler of `system()`.
  void exec_later(resumable* ptr, bool high_prio = true) override;

  /// It is assumed that `this` is never in the neighborhood of `ptr`.
  bool is_neighbor(execution_unit* ptr) const override;

  std::string to_string() const override {
    return "scoped_execution_unit";
  }

};

} // namespace caf

#endif // CAF_SCOPED_EXECUTION_UNIT_HPP
