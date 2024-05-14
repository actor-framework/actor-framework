// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/attachable.hpp"

#include "caf/actor_cast.hpp"
#include "caf/default_attachable.hpp"
#include "caf/system_messages.hpp"

namespace caf {

attachable::~attachable() {
  // Avoid recursive cleanup of next pointers because this can cause a stack
  // overflow for long linked lists.
  using std::swap;
  while (next != nullptr) {
    attachable_ptr tmp;
    swap(next->next, tmp);
    swap(next, tmp);
  }
}

attachable::token::token(size_t typenr, const void* vptr)
  : subtype(typenr), ptr(vptr) {
  // nop
}

void attachable::actor_exited(const error&, scheduler*) {
  // nop
}

bool attachable::matches(const token&) {
  return false;
}

} // namespace caf
