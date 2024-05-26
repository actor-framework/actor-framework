// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

/// Consumes mailbox elements and sends an error message for each request.
struct CAF_CORE_EXPORT sync_request_bouncer {
  // -- constructors, destructors, and assignment operators --------------------

  explicit sync_request_bouncer(error r) : rsn(std::move(r)) {
    // nop
  }

  // -- apply ------------------------------------------------------------------

  void operator()(const strong_actor_ptr& sender, const message_id& mid) const;

  void operator()(const mailbox_element& e) const;

  // -- member variables -------------------------------------------------------

  error rsn;
};

} // namespace caf::detail
