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

#include "caf/actor_addr.hpp"

#include "caf/actor.hpp"
#include "caf/node_id.hpp"
#include "caf/serializer.hpp"
#include "caf/local_actor.hpp"
#include "caf/deserializer.hpp"
#include "caf/proxy_registry.hpp"

namespace caf {

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

intptr_t actor_addr::compare(const abstract_actor* lhs,
                             const abstract_actor* rhs) {
  // invalid actors are always "less" than valid actors
  if (! lhs)
    return rhs ? -1 : 0;
  if (! rhs)
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
  return compare(ptr_.get(), other);
}


actor_id actor_addr::id() const noexcept {
  return (ptr_) ? ptr_->id() : 0;
}

node_id actor_addr::node() const noexcept {
  return ptr_ ? ptr_->node() : node_id{};
}

void actor_addr::swap(actor_addr& other) noexcept {
  ptr_.swap(other.ptr_);
}

void serialize(serializer& sink, actor_addr& x, const unsigned int) {
  sink << actor_cast<channel>(x);
}

void serialize(deserializer& source, actor_addr& x, const unsigned int) {
  channel y;
  source >> y;
  if (! y) {
    x = invalid_actor_addr;
    return;
  }
  if (! y->is_abstract_actor())
    throw std::logic_error("Expected an actor address, found a group address.");
  x.ptr_.reset(static_cast<abstract_actor*>(actor_cast<abstract_channel*>(y)));
}

std::string to_string(const actor_addr& x) {
  if (! x)
    return "<invalid-actor>";
  std::string result = std::to_string(x->id());
  result += "@";
  result += to_string(x->node());
  return result;
}

} // namespace caf
