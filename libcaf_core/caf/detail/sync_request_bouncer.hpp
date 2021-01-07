// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>

#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive/task_result.hpp"

namespace caf::detail {

/// Drains a mailbox and sends an error message to each unhandled request.
struct CAF_CORE_EXPORT sync_request_bouncer {
  error rsn;
  explicit sync_request_bouncer(error r);
  void operator()(const strong_actor_ptr& sender, const message_id& mid) const;

  intrusive::task_result operator()(const mailbox_element& e) const;

  /// Unwrap WDRR queues. Nesting WDRR queues results in a Key/Queue prefix for
  /// each layer of nesting.
  template <class Key, class Queue, class... Ts>
  intrusive::task_result
  operator()(const Key&, const Queue&, const Ts&... xs) const {
    (*this)(xs...);
    return intrusive::task_result::resume;
  }
};

} // namespace caf::detail
