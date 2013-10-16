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


#ifndef CPPA_TYPED_ACTOR_PTR_HPP
#define CPPA_TYPED_ACTOR_PTR_HPP

#include "cppa/replies_to.hpp"
#include "cppa/match_expr.hpp"
#include "cppa/spawn_options.hpp"
#include "cppa/detail/typed_actor_util.hpp"

namespace cppa {

template<typename... Signatures>
class typed_actor_ptr {

 public:

    typedef util::type_list<Signatures...> signatures;

    typed_actor_ptr() = default;
    typed_actor_ptr(typed_actor_ptr&&) = default;
    typed_actor_ptr(const typed_actor_ptr&) = default;
    typed_actor_ptr& operator=(typed_actor_ptr&&) = default;
    typed_actor_ptr& operator=(const typed_actor_ptr&) = default;

    template<typename... Others>
    typed_actor_ptr(typed_actor_ptr<Others...> other) {
        set(std::move(other));
    }

    template<typename... Others>
    typed_actor_ptr& operator=(typed_actor_ptr<Others...> other) {
        set(std::move(other));
        return *this;
    }

    /** @cond PRIVATE */
    static typed_actor_ptr cast_from(actor_ptr from) {
        return {std::move(from)};
    }
    const actor_ptr& type_erased() const { return m_ptr; }
    actor_ptr& type_erased() { return m_ptr; }
    /** @endcond */

 private:

    typed_actor_ptr(actor_ptr ptr) : m_ptr(std::move(ptr)) { }

    template<class ListA, class ListB>
    inline void check_signatures() {
        static_assert(util::tl_is_strict_subset<ListA, ListB>::value,
                      "'this' must be a strict subset of 'other'");
    }

    template<typename... Others>
    inline void set(typed_actor_ptr<Others...> other) {
        check_signatures<signatures, util::type_list<Others...>>();
        m_ptr = std::move(other.type_erased());
    }

    actor_ptr m_ptr;

};

} // namespace cppa

#endif // CPPA_TYPED_ACTOR_PTR_HPP
