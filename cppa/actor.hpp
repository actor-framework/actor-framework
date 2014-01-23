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

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "cppa/intrusive_ptr.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/common_actor_ops.hpp"

#include "cppa/util/comparable.hpp"
#include "cppa/util/type_traits.hpp"

namespace cppa {

class actor_proxy;
class untyped_actor;
class blocking_untyped_actor;

namespace io {
class broker;
} // namespace io

namespace detail {
class raw_access;
} // namespace detail

struct invalid_actor_t { constexpr invalid_actor_t() { } };

constexpr invalid_actor_t invalid_actor = invalid_actor_t{};

/**
 * @brief Identifies an untyped actor.
 */
class actor : util::comparable<actor> {

    friend class detail::raw_access;

 public:

    // common_actor_ops does not provide a virtual destructor -> no new members
    class ops : public common_actor_ops {

        friend class actor;

        typedef common_actor_ops super;

     public:

        ops() = default;

        void enqueue(const message_header& hdr, any_tuple msg) const;

     private:

        inline ops(abstract_actor_ptr ptr) : super(std::move(ptr)) { }

    };

    actor() = default;

    template<typename T>
    actor(intrusive_ptr<T> ptr,
          typename std::enable_if<
                 std::is_base_of<io::broker, T>::value
              || std::is_base_of<actor_proxy, T>::value
              || std::is_base_of<untyped_actor, T>::value
              || std::is_base_of<blocking_untyped_actor, T>::value
          >::type* = 0)
        : m_ops(ptr) { }

    actor(const std::nullptr_t&);

    actor(actor_proxy*);

    actor(untyped_actor*);

    actor(blocking_untyped_actor*);

    actor(const invalid_actor_t&);

    explicit inline operator bool() const;

    inline bool operator!() const;

    inline ops* operator->() const {
        // this const cast is safe, because common_actor_ops cannot be
        // modified anyways and the offered operations are intended to
        // be called on const elements
        return const_cast<ops*>(&m_ops);
    }

    intptr_t compare(const actor& other) const;

 private:

    actor(abstract_actor*);

    ops m_ops;

};

inline actor::operator bool() const {
    return static_cast<bool>(m_ops.m_ptr);
}

inline bool actor::operator!() const {
    return !static_cast<bool>(*this);
}

} // namespace cppa

#endif // CPPA_ACTOR_HPP
