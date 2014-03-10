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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
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

    void enqueue(const message_header& hdr, any_tuple msg) override;

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
