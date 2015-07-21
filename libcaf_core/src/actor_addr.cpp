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
#include "caf/actor_addr.hpp"
#include "caf/local_actor.hpp"

#include "caf/detail/singletons.hpp"

namespace caf {

namespace {

intptr_t compare_impl(const abstract_actor* lhs, const abstract_actor* rhs) {
  return reinterpret_cast<intptr_t>(lhs) - reinterpret_cast<intptr_t>(rhs);
}

} // namespace <anonymous>

actor_addr::actor_addr(const invalid_actor_addr_t&) : ptr_(nullptr) {
  // nop
}

actor_addr::actor_addr(abstract_actor* ptr) : ptr_(ptr) {
  // nop
}

actor_addr actor_addr::operator=(const invalid_actor_addr_t&) {
  ptr_.reset();
  return *this;
}
intptr_t actor_addr::compare(const actor_addr& other) const noexcept {
  return compare_impl(ptr_.get(), other.ptr_.get());
}

intptr_t actor_addr::compare(const abstract_actor* other) const noexcept {
  return compare_impl(ptr_.get(), other);
}


actor_id actor_addr::id() const noexcept {
  return (ptr_) ? ptr_->id() : 0;
}

node_id actor_addr::node() const noexcept {
  return ptr_ ? ptr_->node() : detail::singletons::get_node_id();
}

bool actor_addr::is_remote() const noexcept {
  return ptr_ ? ptr_->is_remote() : false;
}

void actor_addr::swap(actor_addr& other) noexcept {
  ptr_.swap(other.ptr_);
}

} // namespace caf
