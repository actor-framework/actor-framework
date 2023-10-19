// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

actor_id CAF_CORE_EXPORT thread_local_aid() noexcept;

actor_id CAF_CORE_EXPORT thread_local_aid(actor_id aid) noexcept;

/// RAII-style guard that sets the thread-local actor ID to `new_id` in its
/// constructor and restores the old ID in its destructor.
class thread_local_aid_guard {
public:
  explicit thread_local_aid_guard(actor_id new_id) {
    old_id_ = thread_local_aid(new_id);
  }

  ~thread_local_aid_guard() {
    thread_local_aid(old_id_);
  }

private:
  actor_id old_id_;
};

} // namespace caf
