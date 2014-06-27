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


#ifndef CPPA_ACTOR_COMPANION_HPP
#define CPPA_ACTOR_COMPANION_HPP

#include <memory>
#include <functional>

#include "cppa/local_actor.hpp"
#include "cppa/sync_sender.hpp"
#include "cppa/mailbox_based.hpp"
#include "cppa/mailbox_element.hpp"
#include "cppa/behavior_stack_based.hpp"

#include "cppa/detail/memory.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

namespace cppa {

/**
 * @brief An co-existing forwarding all messages through a user-defined
 *        callback to another object, thus serving as gateway to
 *        allow any object to interact with other actors.
 */
class actor_companion : public extend<local_actor, actor_companion>::
                               with<behavior_stack_based<behavior>::impl,
                                    sync_sender<nonblocking_response_handle_tag>::impl> {

    typedef util::shared_spinlock lock_type;

 public:

    typedef std::unique_ptr<mailbox_element, detail::disposer> message_pointer;

    typedef std::function<void (message_pointer)> enqueue_handler;

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

    void enqueue(msg_hdr_cref hdr, message msg, execution_unit* eu) override;

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
typedef intrusive_ptr<actor_companion> actor_companion_ptr;

} // namespace cppa

#endif // CPPA_ACTOR_COMPANION_HPP
