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
#include "cppa/untyped_actor_handle.hpp"

#include "cppa/util/comparable.hpp"

namespace cppa {

class local_actor;
class actor_namespace;

namespace detail { class raw_access; }

/**
 * @brief Identifies an invalid {@link actor_addr}.
 * @relates actor_addr
 */
struct invalid_actor_addr_t { constexpr invalid_actor_addr_t() { } };

constexpr invalid_actor_addr_t invalid_actor_addr = invalid_actor_addr_t{};

/**
 * @brief Stores the address of typed as well as untyped actors.
 */
class actor_addr : util::comparable<actor_addr>
                 , util::comparable<actor_addr, abstract_actor*>
                 , util::comparable<actor_addr, abstract_actor_ptr> {

    friend class abstract_actor;
    friend class detail::raw_access;

 public:

    actor_addr() = default;

    actor_addr(actor_addr&&) = default;

    actor_addr(const actor_addr&) = default;

    actor_addr& operator=(actor_addr&&) = default;

    actor_addr& operator=(const actor_addr&) = default;

    actor_addr(const invalid_actor_addr_t&);

    actor_addr operator=(const invalid_actor_addr_t&);

    inline explicit operator bool() const {
        return valid();
    }

    inline bool operator!() const {
        return !valid();
    }

    inline bool valid() const {
        return m_ops.valid();
    }

    intptr_t compare(const actor_addr& other) const;

    intptr_t compare(const abstract_actor* other) const;

    inline intptr_t compare(const abstract_actor_ptr& other) const {
        return compare(other.get());
    }

    inline const untyped_actor_handle* operator->() const {
        return &m_ops;
    }

    inline const untyped_actor_handle& operator*() const {
        return m_ops;
    }

 private:

    explicit actor_addr(abstract_actor*);

    untyped_actor_handle m_ops;

};

} // namespace cppa

#endif // CPPA_ACTOR_ADDR_HPP
