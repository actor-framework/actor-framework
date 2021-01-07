// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_priority.hpp"
#include "caf/policy/downstream_messages.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/upstream_messages.hpp"
#include "caf/policy/urgent_messages.hpp"
#include "caf/unit.hpp"

namespace caf::policy {

/// Configures a cached WDRR fixed multiplexed queue for dispatching to four
/// nested queue (one for each message category type).
class CAF_CORE_EXPORT categorized {
public:
  // -- member types -----------------------------------------------------------

  using mapped_type = mailbox_element;

  using task_size_type = size_t;

  using deficit_type = size_t;

  using unique_pointer = mailbox_element_ptr;

  // -- constructors, destructors, and assignment operators --------------------

  categorized() = default;

  categorized(const categorized&) = default;

  categorized& operator=(const categorized&) = default;

  constexpr categorized(unit_t) {
    // nop
  }

  // -- interface required by wdrr_fixed_multiplexed_queue ---------------------

  template <template <class> class Queue>
  static deficit_type
  quantum(const Queue<urgent_messages>&, deficit_type x) noexcept {
    // Allow actors to consume twice as many urgent as normal messages per
    // credit round.
    return x + x;
  }

  template <template <class> class Queue>
  static deficit_type
  quantum(const Queue<upstream_messages>& q, deficit_type) noexcept {
    // Allow actors to consume *all* upstream messages. They are lightweight by
    // design and require little processing.
    return q.total_task_size();
  }

  template <class Queue>
  static deficit_type quantum(const Queue&, deficit_type x) noexcept {
    return x;
  }

  static size_t id_of(const mailbox_element& x) noexcept {
    return static_cast<size_t>(x.mid.category());
  }
};

} // namespace caf::policy
