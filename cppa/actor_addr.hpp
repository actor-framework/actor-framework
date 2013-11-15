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


#ifndef CPPA_ACTOR_ADDR_HPP
#define CPPA_ACTOR_ADDR_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "cppa/intrusive_ptr.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/util/comparable.hpp"

namespace cppa {

class actor;
class node_id;
class actor_addr;
class local_actor;

namespace detail { template<typename T> T* actor_addr_cast(const actor_addr&); }

constexpr struct invalid_actor_addr_t { constexpr invalid_actor_addr_t() { } } invalid_actor_addr;

class actor_addr : util::comparable<actor_addr>
                 , util::comparable<actor_addr, actor>
                 , util::comparable<actor_addr, local_actor*> {

    friend class abstract_actor;

    template<typename T>
    friend T* detail::actor_addr_cast(const actor_addr&);

 public:

    actor_addr() = default;
    actor_addr(actor_addr&&) = default;
    actor_addr(const actor_addr&) = default;
    actor_addr& operator=(actor_addr&&) = default;
    actor_addr& operator=(const actor_addr&) = default;

    actor_addr(const actor&);

    actor_addr(const invalid_actor_addr_t&);

    explicit operator bool() const;
    bool operator!() const;

    intptr_t compare(const actor& other) const;
    intptr_t compare(const actor_addr& other) const;
    intptr_t compare(const local_actor* other) const;

    actor_id id() const;

    const node_id& node() const;

    /**
     * @brief Returns whether this is an address of a
     *        remote actor.
     */
    bool is_remote() const;

 private:

    explicit actor_addr(abstract_actor*);

    abstract_actor_ptr m_ptr;

};

namespace detail {

template<typename T>
T* actor_addr_cast(const actor_addr& addr) {
    return static_cast<T*>(addr.m_ptr.get());
}

} // namespace detail

} // namespace cppa

#endif // CPPA_ACTOR_ADDR_HPP
