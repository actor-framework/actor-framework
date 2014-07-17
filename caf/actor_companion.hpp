/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

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
 * @brief An co-existing forwarding all messages through a user-defined
 *        callback to another object, thus serving as gateway to
 *        allow any object to interact with other actors.
 */
class actor_companion : public extend<local_actor, actor_companion>::
                               with<mixin::behavior_stack_based<behavior>::impl,
                                    mixin::sync_sender<nonblocking_response_handle_tag>::impl> {

    using lock_type = detail::shared_spinlock;

 public:

    using message_pointer = std::unique_ptr<mailbox_element, detail::disposer>;

    using enqueue_handler = std::function<void (message_pointer)>;

    /**
     * @brief Removes the handler for incoming messages and terminates
     *        the companion for exit reason @ rsn.
     */
    void disconnect(std::uint32_t rsn = exit_reason::normal);

    /**
     * @brief Sets the handler for incoming messages.
     * @warning @p handler needs to be thread-safe
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
 * @brief A pointer to a co-existing (actor) object.
 * @relates actor_companion
 */
using actor_companion_ptr = intrusive_ptr<actor_companion>;

} // namespace caf

#endif // CAF_ACTOR_COMPANION_HPP
