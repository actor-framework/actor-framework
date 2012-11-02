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


#ifndef CPPA_DEFAULT_ACTOR_ADDRESSING_HPP
#define CPPA_DEFAULT_ACTOR_ADDRESSING_HPP

#include <map>
#include <cstdint>

#include "cppa/actor_proxy.hpp"
#include "cppa/actor_addressing.hpp"
#include "cppa/process_information.hpp"

namespace cppa { namespace network {

class default_protocol;

class default_actor_addressing : public actor_addressing {

 public:

    default_actor_addressing(default_protocol* parent = nullptr);

    typedef std::map<actor_id,weak_actor_proxy_ptr> proxy_map;

    atom_value technology_id() const;

    void write(serializer* sink, const actor_ptr& ptr);

    actor_ptr read(deserializer* source);

    // returns the number of proxy instances for given parent
    size_t count_proxies(const process_information& parent);

    actor_ptr get(const process_information& parent, actor_id aid);

    actor_ptr get_or_put(const process_information& parent, actor_id aid);

    void put(const process_information& parent,
             actor_id aid,
             const actor_proxy_ptr& proxy);

    proxy_map& proxies(process_information& from);

    void erase(process_information& info);

    void erase(process_information& info, actor_id aid);

 private:

    default_protocol* m_parent;
    process_information_ptr m_pinf;
    std::map<process_information,proxy_map> m_proxies;

};

} } // namespace cppa::network

#endif // CPPA_DEFAULT_ACTOR_ADDRESSING_HPP
