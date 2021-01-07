// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/execution_unit.hpp"

namespace caf {

/// Identifies an execution unit, e.g., a worker thread of the scheduler. By
/// querying its execution unit, an actor can access other context information.
class CAF_CORE_EXPORT scoped_execution_unit : public execution_unit {
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
