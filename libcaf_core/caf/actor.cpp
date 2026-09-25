// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor.hpp"

#include "caf/actor_addr.hpp"
#include "caf/scoped_actor.hpp"

namespace caf {

actor::actor(std::nullptr_t) : ptr_(nullptr) {
  // nop
}

actor::actor(const scoped_actor& x) : ptr_(actor_cast<strong_actor_ptr>(x)) {
  // nop
}

actor::actor(actor_control_block* ptr) : ptr_(ptr, add_ref) {
  // nop
}

CAF_PUSH_DEPRECATED_WARNING
actor::actor(actor_control_block* ptr, bool increase_ref_count)
  : ptr_(ptr, increase_ref_count) {
  // nop
}
CAF_POP_WARNINGS

actor::actor(actor_control_block* ptr, add_ref_t) : ptr_(ptr, add_ref) {
  // nop
}

actor::actor(actor_control_block* ptr, adopt_ref_t) : ptr_(ptr, adopt_ref) {
  // nop
}

actor& actor::operator=(std::nullptr_t) {
  ptr_.reset();
  return *this;
}

actor& actor::operator=(const scoped_actor& x) {
  ptr_ = actor_cast<strong_actor_ptr>(x);
  return *this;
}

void actor::swap(actor& other) noexcept {
  ptr_.swap(other.ptr_);
}

actor_addr actor::address() const noexcept {
  if (ptr_) {
    return {ptr_->id(), ptr_->node()};
  }
  return {};
}

bool operator==(const actor& lhs, abstract_actor* rhs) {
  return lhs ? actor_cast<abstract_actor*>(lhs) == rhs : rhs == nullptr;
}

bool operator==(abstract_actor* lhs, const actor& rhs) {
  return rhs == lhs;
}

bool operator!=(const actor& lhs, abstract_actor* rhs) {
  return !(lhs == rhs);
}

bool operator!=(abstract_actor* lhs, const actor& rhs) {
  return !(lhs == rhs);
}

} // namespace caf
