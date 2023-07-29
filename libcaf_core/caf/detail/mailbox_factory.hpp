// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

/// The base class for all mailbox implementations.
class CAF_CORE_EXPORT mailbox_factory {
public:
  virtual ~mailbox_factory();

  /// Creates a new mailbox for `owner`.
  virtual abstract_mailbox* make(scheduled_actor* owner) = 0;

  /// Creates a new mailbox for `owner`.
  virtual abstract_mailbox* make(blocking_actor* owner) = 0;
};

} // namespace caf::detail
