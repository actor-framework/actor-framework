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
#include "cppa/weak_intrusive_ptr.hpp"
#include "cppa/enable_weak_ptr_mixin.hpp"

namespace cppa {

class actor_proxy_cache;

/**
 * @brief Represents a remote actor.
 */
class actor_proxy : public enable_weak_ptr_mixin<actor_proxy,actor> {

    typedef enable_weak_ptr_mixin<actor_proxy,actor> super;

 public:

    /**
     * @brief Establishes a local link state that's not synchronized back
     *        to the remote instance.
     */
    virtual void local_link_to(const intrusive_ptr<actor>& other) = 0;

    /**
     * @brief Removes a local link state.
     */
    virtual void local_unlink_from(const actor_ptr& other) = 0;

 protected:

    actor_proxy(actor_id mid,
                const process_information_ptr& pinfo);

};

/**
 * @brief A smart pointer to an {@link actor_proxy} instance.
 * @relates actor_proxy
 */
typedef intrusive_ptr<actor_proxy> actor_proxy_ptr;

/**
 * @brief A weak smart pointer to an {@link actor_proxy} instance.
 * @relates actor_proxy
 */
typedef weak_intrusive_ptr<actor_proxy> weak_actor_proxy_ptr;

} // namespace cppa

#endif // CPPA_ACTOR_PROXY_HPP
