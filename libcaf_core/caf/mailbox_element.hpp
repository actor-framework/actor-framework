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

// needs access to constructor + destructor to initialize m_dummy_node
namespace caf {

class local_actor;

class mailbox_element : public extend<memory_managed>::
                 with<mixin::memory_cached> {

  friend class local_actor;
  friend class detail::memory;

 public:

  mailbox_element* next; // intrusive next pointer
  bool marked;       // denotes if this node is currently processed
  actor_addr sender;
  message_id mid;
  message msg; // 'content field'

  ~mailbox_element();

  mailbox_element(mailbox_element&&) = delete;
  mailbox_element(const mailbox_element&) = delete;
  mailbox_element& operator=(mailbox_element&&) = delete;
  mailbox_element& operator=(const mailbox_element&) = delete;

  template <class T>
  static mailbox_element* create(actor_addr sender, message_id id, T&& data) {
    return detail::memory::create<mailbox_element>(std::move(sender), id,
                             std::forward<T>(data));
  }

 private:

  mailbox_element() = default;

  mailbox_element(actor_addr sender, message_id id, message data);

};

using unique_mailbox_element_pointer =
  std::unique_ptr<mailbox_element, detail::disposer>;

} // namespace caf

#endif // CAF_RECURSIVE_QUEUE_NODE_HPP
