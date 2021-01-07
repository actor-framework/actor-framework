// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/unit.hpp"

namespace caf::policy {

/// Configures a DRR queue for holding upstream messages.
class CAF_CORE_EXPORT upstream_messages {
public:
  // -- member types -----------------------------------------------------------

  using mapped_type = mailbox_element;

  using task_size_type = size_t;

  using deficit_type = size_t;

  using unique_pointer = mailbox_element_ptr;

  // -- constructors, destructors, and assignment operators --------------------

  upstream_messages() = default;

  upstream_messages(const upstream_messages&) = default;

  upstream_messages& operator=(const upstream_messages&) = default;

  constexpr upstream_messages(unit_t) {
    // nop
  }

  // -- interface required by drr_queue ----------------------------------------

  static task_size_type task_size(const mailbox_element&) noexcept {
    return 1;
  }
};

} // namespace caf::policy
