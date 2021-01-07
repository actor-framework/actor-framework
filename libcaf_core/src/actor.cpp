// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor.hpp"

#include <cassert>
#include <utility>

#include "caf/actor_addr.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/decorator/sequencer.hpp"
#include "caf/deserializer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/local_actor.hpp"
#include "caf/make_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/serializer.hpp"

namespace caf {

actor::actor(std::nullptr_t) : ptr_(nullptr) {
  // nop
}

actor::actor(const scoped_actor& x) : ptr_(actor_cast<strong_actor_ptr>(x)) {
  // nop
}

actor::actor(actor_control_block* ptr) : ptr_(ptr) {
  // nop
}

actor::actor(actor_control_block* ptr, bool add_ref) : ptr_(ptr, add_ref) {
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

intptr_t actor::compare(const actor& x) const noexcept {
  return actor_addr::compare(ptr_.get(), x.ptr_.get());
}

intptr_t actor::compare(const actor_addr& x) const noexcept {
  return actor_addr::compare(ptr_.get(), actor_cast<actor_control_block*>(x));
}

intptr_t actor::compare(const strong_actor_ptr& x) const noexcept {
  return actor_addr::compare(ptr_.get(), x.get());
}

void actor::swap(actor& other) noexcept {
  ptr_.swap(other.ptr_);
}

actor_addr actor::address() const noexcept {
  return actor_cast<actor_addr>(ptr_);
}

actor operator*(actor f, actor g) {
  auto& sys = f->home_system();
  return make_actor<decorator::sequencer, actor>(
    sys.next_actor_id(), sys.node(), &sys,
    actor_cast<strong_actor_ptr>(std::move(f)),
    actor_cast<strong_actor_ptr>(std::move(g)), std::set<std::string>{});
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
