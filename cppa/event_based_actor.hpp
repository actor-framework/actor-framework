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


#ifndef CPPA_UNTYPED_ACTOR_HPP
#define CPPA_UNTYPED_ACTOR_HPP

#include "cppa/on.hpp"
#include "cppa/extend.hpp"
#include "cppa/logging.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/sync_sender.hpp"
#include "cppa/mailbox_based.hpp"
#include "cppa/response_handle.hpp"
#include "cppa/behavior_stack_based.hpp"

#include "cppa/detail/response_handle_util.hpp"

namespace cppa {

/**
 * @brief A cooperatively scheduled, event-based actor implementation.
 *
 * This is the recommended base class for user-defined actors and is used
 * implicitly when spawning functor-based actors without the
 * {@link blocking_api} flag.
 *
 * @extends local_actor
 */
class event_based_actor : public extend<local_actor, event_based_actor>::
                                 with<mailbox_based,
                                      behavior_stack_based<behavior>::impl,
                                      sync_sender<nonblocking_response_handle_tag>::impl> {

 public:

    event_based_actor();

    ~event_based_actor();

 protected:

    /**
     * @brief Returns the initial actor behavior.
     */
    virtual behavior make_behavior() = 0;

    /**
     * @brief Forwards the last received message to @p whom.
     */
    void forward_to(const actor& whom);

};

} // namespace cppa

#endif // CPPA_UNTYPED_ACTOR_HPP
