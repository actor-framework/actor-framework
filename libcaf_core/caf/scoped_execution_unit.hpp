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

#include "caf/execution_unit.hpp"

namespace caf {

/// Identifies an execution unit, e.g., a worker thread of the scheduler. By
/// querying its execution unit, an actor can access other context information.
class scoped_execution_unit : public execution_unit {
public:
  using super = execution_unit;

  using super::super;

  ~scoped_execution_unit() override;

  /// Delegates the resumable to the scheduler of `system()`.
  void exec_later(resumable* ptr) override;

  void system_ptr(actor_system* ptr) noexcept {
    system_ = ptr;
  }
};

} // namespace caf

