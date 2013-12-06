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


#ifndef CPPA_PRIORITY_POLICY_HPP
#define CPPA_PRIORITY_POLICY_HPP

namespace cppa { class mailbox_element; }

namespace cppa { namespace policy {

/**
 * @brief The priority_policy <b>concept</b> class. Please note that this
 *        class is <b>not</b> implemented. It only explains the all
 *        required member function and their behavior for any priority policy.
 */
class priority_policy {

 public:

    /**
     * @brief Returns the next message from the list of cached elements or
     *        @p nullptr. The latter indicates only that there is no element
     *        left in the cache.
     */
    template<class Actor>
    mailbox_element* next_message(Actor* self);

    /**
     * @brief Fetches new messages from the actor's mailbox. The member
     *        function returns @p false if no message was read,
     *        @p true otherwise.
     *
     * This member function calls {@link scheduling_policy::fetch_messages},
     * so a returned @p false value indicates that the client must prepare
     * for re-scheduling.
     */
    template<class Actor, typename F>
    bool fetch_messages(Actor* self);


};

} } // namespace cppa::policy

#endif // CPPA_PRIORITY_POLICY_HPP
