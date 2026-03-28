// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_ref_counted.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/flow/fwd.hpp"

namespace caf::flow {

/// An object that lives on a @ref coordinator.
class CAF_CORE_EXPORT coordinated : public abstract_ref_counted {
public:
  // -- constructors, destructors, and assignment operators --------------------

  virtual ~coordinated();

  // -- properties -------------------------------------------------------------

  /// Returns the @ref coordinator this object lives on.
  virtual coordinator* parent() const noexcept = 0;
};

/// @relates coordinated
using coordinated_ptr = intrusive_ptr<coordinated>;

} // namespace caf::flow
