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


#ifndef ACTOR_PROXY_CACHE_HPP
#define ACTOR_PROXY_CACHE_HPP

#include <string>
#include <vector>
#include <functional>

#include "cppa/actor_proxy.hpp"
#include "cppa/process_information.hpp"
#include "cppa/util/shared_spinlock.hpp"

namespace cppa { namespace detail {

class actor_proxy_cache
{

 public:

    typedef std::tuple<std::uint32_t,                     // actor id
                       std::uint32_t,                     // process id
                       process_information::node_id_type> // node id
            key_tuple;

 private:

    util::shared_spinlock m_lock;
    std::map<key_tuple, actor_proxy_ptr> m_entries;

 public:

    actor_proxy_ptr get(key_tuple const& key);

    // @returns true if pptr was successfully removed, false otherwise
    bool erase(actor_proxy_ptr const& pptr);

};

// get the thread-local cache object
actor_proxy_cache& get_actor_proxy_cache();

} } // namespace cppa::detail

#endif // ACTOR_PROXY_CACHE_HPP
