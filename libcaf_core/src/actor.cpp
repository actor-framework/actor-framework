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

#include "caf/channel.hpp"

#include <utility>

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/serializer.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/bound_actor.hpp"
#include "caf/local_actor.hpp"
#include "caf/deserializer.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/composed_actor.hpp"
#include "caf/event_based_actor.hpp"

namespace caf {

actor::actor(const invalid_actor_t&) : ptr_(nullptr) {
  // nop
}

actor::actor(abstract_actor* ptr) : ptr_(ptr) {
  // nop
}

actor::actor(abstract_actor* ptr, bool add_ref) : ptr_(ptr, add_ref) {
  // nop
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
  return ptr_ ? ptr_->address() : actor_addr{};
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
  return actor_cast<actor>(make_counted<bound_actor>(&ptr_->home_system(),
                                                     address(),
                                                     std::move(msg)));
}

actor operator*(actor f, actor g) {
  if (! f || ! g)
    return invalid_actor;
  auto ptr = make_counted<composed_actor>(&f->home_system(),
                                          f.address(), g.address());
  return actor_cast<actor>(std::move(ptr));
}

void serialize(serializer& sink, actor& x, const unsigned int) {
  sink << x.address();
}

void serialize(deserializer& source, actor& x, const unsigned int) {
  actor_addr addr;
  source >> addr;
  x = actor_cast<actor>(addr);
}

std::string to_string(const actor& x) {
  return to_string(x.address());
}

} // namespace caf
