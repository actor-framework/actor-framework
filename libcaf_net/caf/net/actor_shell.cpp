// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/actor_shell.hpp"

namespace caf::net {

// -- actor_shell --------------------------------------------------------------

actor_shell::~actor_shell() {
  // nop
}

const char* actor_shell::name() const {
  return "caf.net.actor-shell";
}

// -- actor_shell_ptr ----------------------------------------------------------

actor_shell_ptr::actor_shell_ptr(strong_actor_ptr ptr) noexcept
  : ptr_(std::move(ptr)) {
  // nop
}

actor_shell_ptr::~actor_shell_ptr() {
  if (auto ptr = get())
    ptr->quit(exit_reason::normal);
}

actor_shell_ptr::handle_type actor_shell_ptr::as_actor() const noexcept {
  return actor_cast<actor>(ptr_);
}

void actor_shell_ptr::detach(error reason) {
  if (auto ptr = get()) {
    ptr->quit(std::move(reason));
    ptr_.release();
  }
}

actor_shell_ptr::element_type* actor_shell_ptr::get() const noexcept {
  return actor_cast<actor_shell*>(ptr_);
}

} // namespace caf::net
