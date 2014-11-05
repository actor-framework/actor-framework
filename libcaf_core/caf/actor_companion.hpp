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

#ifndef CAF_ACTOR_COMPANION_HPP
#define CAF_ACTOR_COMPANION_HPP

#include <memory>
#include <functional>

#include "caf/local_actor.hpp"
#include "caf/mailbox_element.hpp"

#include "caf/mixin/sync_sender.hpp"
#include "caf/mixin/behavior_stack_based.hpp"

#include "caf/detail/memory.hpp"
#include "caf/detail/shared_spinlock.hpp"

namespace caf {

/**
 * An co-existing forwarding all messages through a user-defined
 * callback to another object, thus serving as gateway to
 * allow any object to interact with other actors.
 */
class actor_companion : public extend<local_actor, actor_companion>::
                 with<mixin::behavior_stack_based<behavior>::impl,
                  mixin::sync_sender<nonblocking_response_handle_tag>::impl> {

  using lock_type = detail::shared_spinlock;

 public:

  using message_pointer = std::unique_ptr<mailbox_element, detail::disposer>;

  using enqueue_handler = std::function<void (message_pointer)>;

  /**
   * Removes the handler for incoming messages and terminates
   * the companion for exit reason @ rsn.
   */
  void disconnect(std::uint32_t rsn = exit_reason::normal);

  /**
   * Sets the handler for incoming messages.
   * @warning `handler` needs to be thread-safe
   */
  void on_enqueue(enqueue_handler handler);

  void enqueue(const actor_addr& sender, message_id mid,
         message content, execution_unit* host) override;

 private:

  // set by parent to define custom enqueue action
  enqueue_handler m_on_enqueue;

  // guards access to m_handler
  lock_type m_lock;

};

/**
 * A pointer to a co-existing (actor) object.
 * @relates actor_companion
 */
using actor_companion_ptr = intrusive_ptr<actor_companion>;

} // namespace caf

#endif // CAF_ACTOR_COMPANION_HPP
