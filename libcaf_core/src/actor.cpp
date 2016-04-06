/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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

#include <utility>

#include "caf/actor_addr.hpp"
#include "caf/make_actor.hpp"
#include "caf/serializer.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/local_actor.hpp"
#include "caf/deserializer.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/event_based_actor.hpp"

#include "caf/decorator/adapter.hpp"
#include "caf/decorator/splitter.hpp"
#include "caf/decorator/sequencer.hpp"

namespace caf {

actor::actor(const scoped_actor& x) : ptr_(actor_cast<strong_actor_ptr>(x)) {
  // nop
}

actor::actor(const invalid_actor_t&) : ptr_(nullptr) {
  // nop
}

actor::actor(actor_control_block* ptr) : ptr_(ptr) {
  // nop
}

actor::actor(actor_control_block* ptr, bool add_ref) : ptr_(ptr, add_ref) {
  // nop
}

actor& actor::operator=(const scoped_actor& x) {
  ptr_ = actor_cast<strong_actor_ptr>(x);
  return *this;
}

actor& actor::operator=(const invalid_actor_t&) {
  ptr_.reset();
  return *this;
}

intptr_t actor::compare(const actor& other) const noexcept {
  return actor_addr::compare(ptr_.get(), other.ptr_.get());
}

intptr_t actor::compare(const actor_addr& other) const noexcept {
  return actor_addr::compare(ptr_.get(), other.ptr_.get());
}

void actor::swap(actor& other) noexcept {
  ptr_.swap(other.ptr_);
}

actor_addr actor::address() const noexcept {
  return actor_cast<actor_addr>(ptr_);
}

node_id actor::node() const noexcept {
  return ptr_ ? ptr_->node() : invalid_node_id;
}

actor_id actor::id() const noexcept {
  return ptr_ ? ptr_->id() : invalid_actor_id;
}

actor actor::bind_impl(message msg) const {
  if (! ptr_)
    return invalid_actor;
  auto& sys = *(ptr_->home_system);
  return make_actor<decorator::adapter, actor>(sys.next_actor_id(), sys.node(),
                                               &sys, ptr_, std::move(msg));
}

actor operator*(actor f, actor g) {
  if (! f || ! g)
    return invalid_actor;
  auto& sys = f->home_system();
  return make_actor<decorator::sequencer, actor>(
    sys.next_actor_id(), sys.node(), &sys,
    actor_cast<strong_actor_ptr>(std::move(f)),
    actor_cast<strong_actor_ptr>(std::move(g)), std::set<std::string>{});
}

actor actor::splice_impl(std::initializer_list<actor> xs) {
  if (xs.size() < 2)
    return invalid_actor;
  actor_system* sys = nullptr;
  std::vector<strong_actor_ptr> tmp;
  for (auto& x : xs)
    if (x) {
      if (! sys)
        sys = &(x->home_system());
      tmp.push_back(actor_cast<strong_actor_ptr>(x));
    } else {
      return invalid_actor;
    }
  return make_actor<decorator::splitter, actor>(sys->next_actor_id(),
                                                sys->node(), sys,
                                                std::move(tmp),
                                                std::set<std::string>{});
}

bool operator==(const actor& lhs, abstract_actor* rhs) {
  return actor_cast<abstract_actor*>(lhs) == rhs;
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
