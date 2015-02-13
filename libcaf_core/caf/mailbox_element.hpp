/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_RECURSIVE_QUEUE_NODE_HPP
#define CAF_RECURSIVE_QUEUE_NODE_HPP

#include <cstdint>

#include "caf/extend.hpp"
#include "caf/message.hpp"
#include "caf/actor_addr.hpp"
#include "caf/message_id.hpp"
#include "caf/ref_counted.hpp"

#include "caf/mixin/memory_cached.hpp"

namespace caf {

class mailbox_element : public extend<memory_managed>::
                               with<mixin::memory_cached> {
 public:
  mailbox_element* next; // intrusive next pointer
  mailbox_element* prev; // intrusive previous pointer
  bool marked;           // denotes if this node is currently processed
  actor_addr sender;
  message_id mid;
  message msg;           // 'content field'

  mailbox_element();
  mailbox_element(actor_addr sender, message_id id);
  mailbox_element(actor_addr sender, message_id id, message data);

  ~mailbox_element();

  mailbox_element(mailbox_element&&) = delete;
  mailbox_element(const mailbox_element&) = delete;
  mailbox_element& operator=(mailbox_element&&) = delete;
  mailbox_element& operator=(const mailbox_element&) = delete;

  using unique_ptr = std::unique_ptr<mailbox_element, detail::disposer>;

  static unique_ptr create(actor_addr sender, message_id id, message msg);

  inline bool is_high_priority() const {
    return mid.is_high_priority();
  }
};

using mailbox_element_ptr = std::unique_ptr<mailbox_element, detail::disposer>;

} // namespace caf

#endif // CAF_RECURSIVE_QUEUE_NODE_HPP
