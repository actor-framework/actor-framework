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


#ifndef CPPA_TYPED_ACTOR_HPP
#define CPPA_TYPED_ACTOR_HPP

#include "cppa/actor_addr.hpp"
#include "cppa/replies_to.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/typed_behavior.hpp"

namespace cppa {

class actor_addr;
class local_actor;

struct invalid_actor_addr_t;

template<typename... Signatures>
class typed_event_based_actor;

/**
 * @brief Identifies a strongly typed actor.
 */
template<typename... Signatures>
class typed_actor : util::comparable<typed_actor<Signatures...>>
                  , util::comparable<typed_actor<Signatures...>, actor_addr>
                  , util::comparable<typed_actor<Signatures...>, invalid_actor_addr_t> {

    friend class local_actor;

 public:

    /**
     * @brief Identifies the behavior type actors of this kind use
     *        for their behavior stack.
     */
    typedef typed_behavior<Signatures...> behavior_type;

    /**
     * @brief Identifies pointers to instances of this kind of actor.
     */
    typedef typed_event_based_actor<Signatures...>* pointer;

    /**
     * @brief Identifies the base class for this kind of actor.
     */
    typedef typed_event_based_actor<Signatures...> impl;

    typedef util::type_list<Signatures...> signatures;

    typed_actor() = default;
    typed_actor(typed_actor&&) = default;
    typed_actor(const typed_actor&) = default;
    typed_actor& operator=(typed_actor&&) = default;
    typed_actor& operator=(const typed_actor&) = default;

    template<typename... OtherSignatures>
    typed_actor(const typed_actor<OtherSignatures...>& other) {
        set(std::move(other));
    }

    template<typename... OtherSignatures>
    typed_actor& operator=(const typed_actor<OtherSignatures...>& other) {
        set(std::move(other));
        return *this;
    }

    template<class Impl>
    typed_actor(intrusive_ptr<Impl> other) {
        set(other);
    }

    /**
     * @brief Queries the address of the stored actor.
     */
    actor_addr address() const {
        return m_ptr ? m_ptr->address() : actor_addr{};
    }

    inline intptr_t compare(const actor_addr& rhs) const {
        return address().compare(rhs);
    }

    inline intptr_t compare(const invalid_actor_addr_t&) const {
        return m_ptr ? 1 : 0;
    }

 private:

    template<class ListA, class ListB>
    inline void check_signatures() {
        static_assert(util::tl_is_strict_subset<ListA, ListB>::value,
                      "'this' must be a strict subset of 'other'");
    }

    template<typename... OtherSignatures>
    inline void set(const typed_actor<OtherSignatures...>& other) {
        check_signatures<signatures, util::type_list<OtherSignatures...>>();
        m_ptr = other.m_ptr;
    }

    template<class Impl>
    inline void set(intrusive_ptr<Impl>& other) {
        check_signatures<signatures, typename Impl::signatures>();
        m_ptr = std::move(other);
    }

    abstract_actor_ptr m_ptr;

};

} // namespace cppa

#endif // CPPA_TYPED_ACTOR_HPP
