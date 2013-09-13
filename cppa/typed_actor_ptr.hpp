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
class typed_actor_ptr;

// functions that need access to typed_actor_ptr::unbox()
template<spawn_options Options, typename... Ts>
typed_actor_ptr<typename detail::deduce_signature<Ts>::type...>
spawn_typed(const match_expr<Ts...>&);

template<typename... Ts>
void send_exit(const typed_actor_ptr<Ts...>&, std::uint32_t);

template<typename... Ts, typename... Us>
typed_message_future<
    typename detail::deduce_output_type<
        util::type_list<Ts...>,
        util::type_list<
            typename detail::implicit_conversions<
                typename util::rm_const_and_ref<
                    Us
                >::type
            >::type...
        >
    >::type
>
sync_send(const typed_actor_ptr<Ts...>&, Us&&...);

template<typename... Signatures>
class typed_actor_ptr {

    template<spawn_options Options, typename... Ts>
    friend typed_actor_ptr<typename detail::deduce_signature<Ts>::type...>
           spawn_typed(const match_expr<Ts...>&);

    template<typename... Ts>
    friend void send_exit(const typed_actor_ptr<Ts...>&, std::uint32_t);

    template<typename... Ts, typename... Us>
    friend typed_message_future<
               typename detail::deduce_output_type<
                   util::type_list<Ts...>,
                   util::type_list<
                       typename detail::implicit_conversions<
                           typename util::rm_const_and_ref<
                               Us
                           >::type
                       >::type...
                   >
               >::type
           >
           sync_send(const typed_actor_ptr<Ts...>&, Us&&...);

 public:

    typedef util::type_list<Signatures...> signatures;

    typed_actor_ptr() = default;
    typed_actor_ptr(typed_actor_ptr&&) = default;
    typed_actor_ptr(const typed_actor_ptr&) = default;
    typed_actor_ptr& operator=(typed_actor_ptr&&) = default;
    typed_actor_ptr& operator=(const typed_actor_ptr&) = default;

 private:

    const actor_ptr& unbox() const { return m_ptr; }

    typed_actor_ptr(actor_ptr ptr) : m_ptr(std::move(ptr)) { }

    actor_ptr m_ptr;

};

} // namespace cppa

#endif // CPPA_TYPED_ACTOR_PTR_HPP
