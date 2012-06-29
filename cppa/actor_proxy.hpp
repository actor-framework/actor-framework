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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
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


#ifndef CPPA_ACTOR_PROXY_HPP
#define CPPA_ACTOR_PROXY_HPP

#include "cppa/actor.hpp"
#include "cppa/detail/abstract_actor.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Represents a remote actor.
 */
class actor_proxy : public actor { };

#else // CPPA_DOCUMENTATION

class actor_proxy : public detail::abstract_actor<actor> {

    typedef abstract_actor<actor> super;

 public:

    actor_proxy(std::uint32_t mid, const process_information_ptr& parent);

    void enqueue(actor* sender, any_tuple msg);

    void link_to(intrusive_ptr<actor>& other);

    // do not cause to send this actor an "UNLINK" message
    // to the "original" remote actor
    void local_link_to(intrusive_ptr<actor>& other);

    void unlink_from(intrusive_ptr<actor>& other);

    // do not cause to send this actor an "UNLINK" message
    // to the "original" remote actor
    void local_unlink_from(intrusive_ptr<actor>& other);

    bool remove_backlink(intrusive_ptr<actor>& to);

    bool establish_backlink(intrusive_ptr<actor>& to);

 public:

    void forward_message(const process_information_ptr&, actor*, any_tuple&&);

};

#endif // CPPA_DOCUMENTATION

/**
 * @brief A smart pointer to an {@link actor_proxy} instance.
 * @relates actor_proxy
 */
typedef intrusive_ptr<actor_proxy> actor_proxy_ptr;

} // namespace cppa

#endif // CPPA_ACTOR_PROXY_HPP
