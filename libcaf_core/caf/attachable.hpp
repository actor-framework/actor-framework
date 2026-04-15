// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <memory>

namespace caf::internal {

class attachable_predicate;

} // namespace caf::internal

namespace caf {

/// @relates attachable
using attachable_ptr = std::unique_ptr<attachable>;

/// Callback utility class.
class CAF_CORE_EXPORT attachable {
public:
  // -- constructors and destructors -------------------------------------------

  attachable() = default;

  attachable(const attachable&) = delete;

  attachable& operator=(const attachable&) = delete;

  virtual ~attachable() noexcept;

  // -- interface for the actor ------------------------------------------------

  /// Executed if the actor finished execution with given `reason`.
  /// @warning `sched` can be `nullptr`
  virtual void
  actor_exited(abstract_actor* self, const error& reason, scheduler* sched)
    = 0;

  /// Returns `true` if `what` selects this instance, otherwise `false`.
  virtual bool matches(const internal::attachable_predicate& pred) const;

  // -- member variables -------------------------------------------------------

  attachable_ptr next;
};

} // namespace caf
