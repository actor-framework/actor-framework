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


#ifndef CPPA_ACTOR_HPP
#define CPPA_ACTOR_HPP

#include <cstdint>
#include <type_traits>

#include "cppa/intrusive_ptr.hpp"
#include "cppa/abstract_actor.hpp"

#include "cppa/util/comparable.hpp"

namespace cppa {

class channel;
class any_tuple;
class actor_addr;
class local_actor;
class message_header;

/**
 * @brief Identifies an untyped actor.
 */
class actor : util::comparable<actor> {

    friend class channel;
    friend class actor_addr;
    friend class local_actor;

 public:

    actor() = default;

    template<typename T>
    actor(intrusive_ptr<T> ptr, typename std::enable_if<std::is_base_of<abstract_actor, T>::value>::type* = 0) : m_ptr(ptr) { }

    actor(abstract_actor*);

    actor_id id() const;

    actor_addr address() const;

    explicit operator bool() const;

    bool operator!() const;

    void enqueue(const message_header& hdr, any_tuple msg) const;

    bool is_remote() const;

    intptr_t compare(const actor& other) const;

 private:

    intrusive_ptr<abstract_actor> m_ptr;

};

} // namespace cppa

#endif // CPPA_ACTOR_HPP
