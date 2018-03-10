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

#include <vector>

#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"

#include "caf/detail/dynamic_message_data.hpp"

namespace caf {

message_builder::message_builder() {
  init();
}

message_builder::~message_builder() {
  // nop
}

void message_builder::init() {
  // this should really be done by delegating
  // constructors, but we want to support
  // some compilers without that feature...
  data_ = make_counted<detail::dynamic_message_data>();
}

void message_builder::clear() {
  data()->clear();
}

size_t message_builder::size() const {
  return data()->size();
}

bool message_builder::empty() const {
  return size() == 0;
}

message_builder& message_builder::emplace(type_erased_value_ptr x) {
  data()->append(std::move(x));
  return *this;
}

message message_builder::to_message() const {
  // this const_cast is safe, because the message is
  // guaranteed to detach its data before modifying it
  detail::message_data::cow_ptr ptr;
  ptr.reset(const_cast<detail::dynamic_message_data*>(data()));
  return message{std::move(ptr)};
}

message message_builder::move_to_message() {
  message result;
  using pointer = detail::dynamic_message_data*;
  result.vals().reset(static_cast<pointer>(data_.detach()), false);
  return result;
}

message message_builder::extract(message_handler f) const {
  return to_message().extract(std::move(f));
}

optional<message> message_builder::apply(message_handler handler) {
  // avoid detaching of data_ by moving the data to a message object,
  // calling message::apply and moving the data back
  message::data_ptr ptr;
  ptr.reset(static_cast<detail::dynamic_message_data*>(data_.detach()), false);
  message msg{std::move(ptr)};
  auto res = msg.apply(std::move(handler));
  data_.reset(msg.vals().release(), false);
  return res;
}

detail::dynamic_message_data* message_builder::data() {
  // detach if needed, i.e., assume further non-const
  // operations on data_ can cause race conditions if
  // someone else holds a reference to data_
  if (!data_->unique()) {
    auto tmp = static_cast<detail::dynamic_message_data*>(data_.get())->copy();
    data_.reset(tmp.release(), false);
  }
  return static_cast<detail::dynamic_message_data*>(data_.get());
}

const detail::dynamic_message_data* message_builder::data() const {
  return static_cast<const detail::dynamic_message_data*>(data_.get());
}

} // namespace caf
