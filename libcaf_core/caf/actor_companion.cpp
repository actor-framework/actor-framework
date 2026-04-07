// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_companion.hpp"

#include "caf/detail/assert.hpp"
#include "caf/detail/critical.hpp"

namespace caf {

actor_companion::~actor_companion() {
  // nop
}

void actor_companion::on_enqueue(enqueue_handler handler) {
  std::lock_guard guard{mtx_};
  if (!closed_) {
    on_enqueue_ = std::move(handler);
  }
}

void actor_companion::on_exit(on_exit_handler handler) {
  std::lock_guard guard{mtx_};
  if (!closed_) {
    on_exit_ = std::move(handler);
  }
}

bool actor_companion::enqueue(mailbox_element_ptr what, scheduler*) {
  CAF_ASSERT(what);
  std::shared_lock guard{mtx_};
  if (on_enqueue_) {
    on_enqueue_(std::move(what));
    return true;
  }
  return false;
}

const char* actor_companion::name() const {
  return "caf.actor-companion";
}

void actor_companion::on_cleanup(const error&) {
  on_exit_handler handler;
  {
    std::lock_guard guard{mtx_};
    handler.swap(on_exit_);
  }
  if (handler) {
    handler();
  }
}

bool actor_companion::try_force_close_mailbox() {
  auto result = false;
  enqueue_handler tmp;
  {
    std::unique_lock guard(mtx_);
    if (!closed_) {
      closed_ = true;
      result = true;
      on_enqueue_.swap(tmp);
    }
  }
  return result;
}

} // namespace caf
