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

#include <utility>

#include "caf/actor.hpp"
#include "caf/channel.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/local_actor.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/event_based_actor.hpp"

namespace caf {

actor::actor(const invalid_actor_t&) : ptr_(nullptr) {
  // nop
}

actor::actor(abstract_actor* ptr) : ptr_(ptr) {
  // nop
}

actor& actor::operator=(const invalid_actor_t&) {
  ptr_.reset();
  return *this;
}

intptr_t actor::compare(const actor& other) const noexcept {
  return channel::compare(ptr_.get(), other.ptr_.get());
}

intptr_t actor::compare(const actor_addr& other) const noexcept {
  return static_cast<ptrdiff_t>(ptr_.get() - other.ptr_.get());
}

void actor::swap(actor& other) noexcept {
  ptr_.swap(other.ptr_);
}

actor_addr actor::address() const noexcept {
  return ptr_ ? ptr_->address() : actor_addr{};
}

bool actor::is_remote() const noexcept {
  return ptr_ ? ptr_->is_remote() : false;
}

actor_id actor::id() const noexcept {
  return ptr_ ? ptr_->id() : invalid_actor_id;
}

} // namespace caf
