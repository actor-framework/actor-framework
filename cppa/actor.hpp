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
#include <utility>
#include <type_traits>

#include "cppa/intrusive_ptr.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/untyped_actor_handle.hpp"

#include "cppa/util/comparable.hpp"
#include "cppa/util/type_traits.hpp"

namespace cppa {

class actor_addr;
class actor_proxy;
class blocking_actor;
class event_based_actor;

struct invalid_actor_addr_t;

namespace io {
class broker;
} // namespace io

namespace detail {
class raw_access;
} // namespace detail

struct invalid_actor_t { constexpr invalid_actor_t() { } };

/**
 * @brief Identifies an invalid {@link actor}.
 * @relates actor
 */
constexpr invalid_actor_t invalid_actor = invalid_actor_t{};

/**
 * @brief Identifies an untyped actor.
 *
 * Can be used with derived types of {@link event_based_actor},
 * {@link blocking_actor}, {@link actor_proxy}, or
 * {@link io::broker}.
 */
class actor : util::comparable<actor>
            , util::comparable<actor, actor_addr>
            , util::comparable<actor, invalid_actor_t>
            , util::comparable<actor, invalid_actor_addr_t> {

    friend class detail::raw_access;

 public:

    /**
     * @brief Extends {@link untyped_actor_handle} to grant
     *        access to the enqueue member function.
     */
    class handle : public untyped_actor_handle {

        // untyped_actor_handle does not provide a virtual destructor
        // -> no new members

        friend class actor;

        typedef untyped_actor_handle super;

     public:

        handle() = default;

        /**
         * @brief Enqueues @p msg to the receiver specified in @p hdr.
         */
        void enqueue(const message_header& hdr, any_tuple msg) const;

     private:

        inline handle(abstract_actor_ptr ptr) : super(std::move(ptr)) { }

    };

    actor() = default;

    actor(actor&&) = default;

    actor(const actor&) = default;

    template<typename T>
    actor(intrusive_ptr<T> ptr,
          typename std::enable_if<
                 std::is_base_of<io::broker, T>::value
              || std::is_base_of<actor_proxy, T>::value
              || std::is_base_of<blocking_actor, T>::value
              || std::is_base_of<event_based_actor, T>::value
          >::type* = 0)
        : m_ops(std::move(ptr)) { }

    template<typename T>
    actor(T* ptr,
          typename std::enable_if<
                 std::is_base_of<io::broker, T>::value
              || std::is_base_of<actor_proxy, T>::value
              || std::is_base_of<event_based_actor, T>::value
              || std::is_base_of<blocking_actor, T>::value
          >::type* = 0)
        : m_ops(ptr) { }

    actor(const invalid_actor_t&);

    actor& operator=(actor&&) = default;

    actor& operator=(const actor&) = default;

    template<typename T>
    typename std::enable_if<
           std::is_base_of<io::broker, T>::value
        || std::is_base_of<actor_proxy, T>::value
        || std::is_base_of<event_based_actor, T>::value
        || std::is_base_of<blocking_actor, T>::value,
        actor&
    >::type
    operator=(intrusive_ptr<T> ptr) {
        actor tmp{std::move(ptr)};
        swap(tmp);
        return *this;
    }

    template<typename T>
    typename std::enable_if<
           std::is_base_of<io::broker, T>::value
        || std::is_base_of<actor_proxy, T>::value
        || std::is_base_of<event_based_actor, T>::value
        || std::is_base_of<blocking_actor, T>::value,
        actor&
    >::type
    operator=(T* ptr) {
        actor tmp{ptr};
        swap(tmp);
        return *this;
    }

    actor& operator=(const invalid_actor_t&);

    inline operator bool() const {
        return static_cast<bool>(m_ops.m_ptr);
    }

    inline bool operator!() const {
        return !static_cast<bool>(m_ops.m_ptr);
    }

    /**
     * @brief Queries whether this handle is valid, i.e., points
     *        to an instance of an untyped actor.
     */
    inline bool valid() const {
        return static_cast<bool>(m_ops.m_ptr);
    }

    /**
     * @brief Returns a handle that grants access to
     *        actor operations such as enqueue.
     */
    inline const handle* operator->() const {
        return &m_ops;
    }

    inline const handle& operator*() const {
        return m_ops;
    }

    intptr_t compare(const actor& other) const;

    intptr_t compare(const invalid_actor_t&) const;

    intptr_t compare(const actor_addr&) const;

    inline intptr_t compare(const invalid_actor_addr_t&) const {
        return compare(invalid_actor);
    }

 private:

    void swap(actor& other);

    actor(abstract_actor*);

    handle m_ops;

};

} // namespace cppa

#endif // CPPA_ACTOR_HPP
