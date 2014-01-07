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
#include "cppa/common_actor_ops.hpp"

#include "cppa/util/comparable.hpp"

namespace cppa {

class actor;
class local_actor;
namespace detail { class raw_access; }

struct invalid_actor_addr_t { constexpr invalid_actor_addr_t() { } };

constexpr invalid_actor_addr_t invalid_actor_addr = invalid_actor_addr_t{};

class actor_addr : util::comparable<actor_addr>
                 , util::comparable<actor_addr, actor>
                 , util::comparable<actor_addr, local_actor*> {

    friend class abstract_actor;
    friend class detail::raw_access;

 public:

    actor_addr() = default;
    actor_addr(actor_addr&&) = default;
    actor_addr(const actor_addr&) = default;
    actor_addr& operator=(actor_addr&&) = default;
    actor_addr& operator=(const actor_addr&) = default;

    actor_addr(const actor&);
    actor_addr& operator=(const actor&);

    actor_addr(const invalid_actor_addr_t&);
    actor_addr operator=(const invalid_actor_addr_t&);

    explicit operator bool() const;
    bool operator!() const;

    intptr_t compare(const actor& other) const;
    intptr_t compare(const actor_addr& other) const;
    intptr_t compare(const local_actor* other) const;

    inline common_actor_ops* operator->() const {
        // this const cast is safe, because common_actor_ops cannot be
        // modified anyways and the offered operations are intended to
        // be called on const elements
        return const_cast<common_actor_ops*>(&m_ops);
    }

 private:

    explicit actor_addr(abstract_actor*);

    common_actor_ops m_ops;

};

} // namespace cppa

#endif // CPPA_ACTOR_ADDR_HPP
