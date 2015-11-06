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

#include "caf/group.hpp"

#include "caf/channel.hpp"
#include "caf/message.hpp"
#include "caf/actor_cast.hpp"
#include "caf/group_manager.hpp"

namespace caf {

group::group(abstract_group* ptr) : ptr_(ptr) {
  // nop
}

group::group(const invalid_group_t&) : ptr_(nullptr) {
  // nop
}

group::group(abstract_group_ptr gptr) : ptr_(std::move(gptr)) {
  // nop
}

group& group::operator=(const invalid_group_t&) {
  ptr_.reset();
  return *this;
}

intptr_t group::compare(const group& other) const noexcept {
  return channel::compare(ptr_.get(), other.ptr_.get());
}

void serialize(serializer& sink, const group& x, const unsigned int) {
  sink << actor_cast<channel>(x);
}

void serialize(deserializer& source, group& x, const unsigned int) {
  channel y;
  source >> y;
  if (! y) {
    x = invalid_group;
    return;
  }
  if (! y->is_abstract_group())
    throw std::logic_error("Expected an actor address, found a group address.");
  x.ptr_.reset(static_cast<abstract_group*>(actor_cast<abstract_channel*>(y)));
}

std::string to_string(const group& x) {
  if (x == invalid_group)
    return "<invalid-group>";
  std::string result = x->get_module()->name();
  result += "/";
  result += x->identifier();
  return result;
}

} // namespace caf
