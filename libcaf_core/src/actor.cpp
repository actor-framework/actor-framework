/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/actor.hpp"

#include <cassert>
#include <utility>

#include "caf/actor_addr.hpp"
#include "caf/make_actor.hpp"
#include "caf/serializer.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/local_actor.hpp"
#include "caf/deserializer.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/event_based_actor.hpp"

#include "caf/decorator/splitter.hpp"
#include "caf/decorator/sequencer.hpp"

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

actor actor::splice_impl(std::initializer_list<actor> xs) {
  assert(xs.size() >= 2);
  actor_system* sys = nullptr;
  std::vector<strong_actor_ptr> tmp;
  for (auto& x : xs) {
    if (sys == nullptr)
      sys = &(x->home_system());
    tmp.push_back(actor_cast<strong_actor_ptr>(x));
  }
  return make_actor<decorator::splitter, actor>(sys->next_actor_id(),
                                                sys->node(), sys,
                                                std::move(tmp),
                                                std::set<std::string>{});
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
