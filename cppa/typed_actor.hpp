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

template<typename... Rs>
class typed_event_based_actor;

/**
 * @brief Identifies a strongly typed actor.
 * @tparam Rs Interface as @p replies_to<...>::with<...> parameter pack.
 */
template<typename... Rs>
class typed_actor : util::comparable<typed_actor<Rs...>>
                  , util::comparable<typed_actor<Rs...>, actor_addr>
                  , util::comparable<typed_actor<Rs...>, invalid_actor_addr_t> {

    friend class local_actor;

    // friend with all possible instantiations
    template<typename...>
    friend class typed_actor;

 public:

    /**
     * @brief Identifies the behavior type actors of this kind use
     *        for their behavior stack.
     */
    typedef typed_behavior<Rs...> behavior_type;

    /**
     * @brief Identifies pointers to instances of this kind of actor.
     */
    typedef typed_event_based_actor<Rs...>* pointer;

    /**
     * @brief Identifies the base class for this kind of actor.
     */
    typedef typed_event_based_actor<Rs...> base;

    typedef util::type_list<Rs...> signatures;

    typed_actor() = default;
    typed_actor(typed_actor&&) = default;
    typed_actor(const typed_actor&) = default;
    typed_actor& operator=(typed_actor&&) = default;
    typed_actor& operator=(const typed_actor&) = default;

    template<typename... OtherRs>
    typed_actor(const typed_actor<OtherRs...>& other) {
        set(std::move(other));
    }

    template<typename... OtherRs>
    typed_actor& operator=(const typed_actor<OtherRs...>& other) {
        set(std::move(other));
        return *this;
    }

    template<class Impl>
    typed_actor(intrusive_ptr<Impl> other) {
        set(other);
    }

    pointer operator->() const {
        return static_cast<pointer>(m_ptr.get());
    }

    base& operator*() const {
        return static_cast<base&>(*m_ptr.get());
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

    inline intptr_t compare(const typed_actor& other) const {
        return compare(other.address());
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

    template<typename... OtherRs>
    inline void set(const typed_actor<OtherRs...>& other) {
        check_signatures<signatures, util::type_list<OtherRs...>>();
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
