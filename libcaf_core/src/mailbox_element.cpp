/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include "caf/mailbox_element.hpp"

namespace caf {

mailbox_element_wrapper::mailbox_element_wrapper(strong_actor_ptr&& x0,
                                                 message_id x1,
                                                 forwarding_stack&& x2,
                                                 message&& x3)
    : mailbox_element(std::move(x0), x1, std::move(x2)), msg(std::move(x3)) {
  // nop
}

type_erased_tuple& mailbox_element_wrapper::content() {
  auto ptr = msg.vals().raw_ptr();
  if (ptr != nullptr)
    return *ptr;
  return dummy_;
}

message mailbox_element_wrapper::move_content_to_message() {
  return std::move(msg);
}

message mailbox_element_wrapper::copy_content_to_message() const {
  return msg;
}

mailbox_element::mailbox_element()
    : next(nullptr),
      prev(nullptr),
      marked(false) {
  // nop
}

mailbox_element::mailbox_element(strong_actor_ptr&& x, message_id y,
                                 forwarding_stack&& z)
    : next(nullptr),
      prev(nullptr),
      marked(false),
      sender(std::move(x)),
      mid(y),
      stages(std::move(z)) {
  // nop
}

mailbox_element::~mailbox_element() {
  // nop
}

type_erased_tuple& mailbox_element::content() {
  return dummy_;
}

message mailbox_element::move_content_to_message() {
  return {};
}

message mailbox_element::copy_content_to_message() const {
  return {};
}

const type_erased_tuple& mailbox_element::content() const {
  return const_cast<mailbox_element*>(this)->content();
}

mailbox_element_ptr make_mailbox_element(strong_actor_ptr sender, message_id id,
                                         mailbox_element::forwarding_stack stages,
                                         message msg) {
  auto ptr = new mailbox_element_wrapper(std::move(sender), id,
                                         std::move(stages), std::move(msg));
  return mailbox_element_ptr{ptr};
}

} // namespace caf
