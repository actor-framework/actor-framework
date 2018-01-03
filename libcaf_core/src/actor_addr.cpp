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

#include "caf/actor_addr.hpp"

#include "caf/actor.hpp"
#include "caf/node_id.hpp"
#include "caf/serializer.hpp"
#include "caf/local_actor.hpp"
#include "caf/deserializer.hpp"
#include "caf/proxy_registry.hpp"

namespace caf {

actor_addr::actor_addr(std::nullptr_t) {
  // nop
}

actor_addr::actor_addr(const unsafe_actor_handle_init_t&) {
  // nop
}

actor_addr& actor_addr::operator=(std::nullptr_t) {
  ptr_.reset();
  return *this;
}

actor_addr::actor_addr(actor_control_block* ptr) : ptr_(ptr) {
  // nop
}

actor_addr::actor_addr(actor_control_block* ptr, bool add_ref)
    : ptr_(ptr, add_ref) {
  // nop
}

intptr_t actor_addr::compare(const actor_control_block* lhs,
                             const actor_control_block* rhs) {
  // invalid actors are always "less" than valid actors
  if (lhs == nullptr)
    return rhs != nullptr ? -1 : 0;
  if (rhs == nullptr)
    return 1;
  // check for identity
  if (lhs == rhs)
    return 0;
  // check for equality (a decorator is equal to the actor it represents)
  auto x = lhs->id();
  auto y = rhs->id();
  if (x == y)
    return lhs->node().compare(rhs->node());
  return static_cast<intptr_t>(x) - static_cast<intptr_t>(y);
}

intptr_t actor_addr::compare(const actor_addr& other) const noexcept {
  return compare(ptr_.get(), other.ptr_.get());
}

intptr_t actor_addr::compare(const abstract_actor* other) const noexcept {
  return compare(ptr_.get(), actor_control_block::from(other));
}

intptr_t actor_addr::compare(const actor_control_block* other) const noexcept {
  return compare(ptr_.get(), other);
}

void actor_addr::swap(actor_addr& other) noexcept {
  ptr_.swap(other.ptr_);
}

} // namespace caf
