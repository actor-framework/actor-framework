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

#include "caf/mailbox_element.hpp"

namespace caf {

mailbox_element::mailbox_element()
    : next(nullptr),
      prev(nullptr),
      marked(false) {
  // nop
}

mailbox_element::mailbox_element(actor_addr x, message_id y, forwarding_stack z)
    : next(nullptr),
      prev(nullptr),
      marked(false),
      sender(std::move(x)),
      mid(y),
      stages(std::move(z)) {
  // nop
}

mailbox_element::mailbox_element(actor_addr x, message_id y,
                                 forwarding_stack z, message m)
    : next(nullptr),
      prev(nullptr),
      marked(false),
      sender(std::move(x)),
      mid(y),
      stages(std::move(z)),
      msg(std::move(m)) {
  // nop
}

mailbox_element::~mailbox_element() {
  // nop
}

mailbox_element_ptr mailbox_element::make(actor_addr sender, message_id id,
                                          forwarding_stack stages,
                                          message msg) {
  auto ptr = detail::memory::create<mailbox_element>(std::move(sender), id,
                                                     std::move(stages),
                                                     std::move(msg));
  return mailbox_element_ptr{ptr};
}

std::string to_string(const mailbox_element& x) {
  std::string result = "(";
  result += to_string(x.sender);
  result += ", ";
  result += to_string(x.mid);
  result += ", ";
  result += to_string(x.msg);
  result += ")";
  return result;
}

} // namespace caf
