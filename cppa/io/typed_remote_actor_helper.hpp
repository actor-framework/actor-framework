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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CPPA_IO_TYPED_REMOTE_ACTOR_HELPER_HPP
#define CPPA_IO_TYPED_REMOTE_ACTOR_HELPER_HPP

#include "cppa/actor_cast.hpp"
#include "cppa/typed_actor.hpp"

#include "cppa/typed_actor.hpp"

#include "cppa/detail/type_list.hpp"

#include "cppa/io/remote_actor_impl.hpp"

namespace cppa {
namespace io {

template<class List>
struct typed_remote_actor_helper;

template<typename... Ts>
struct typed_remote_actor_helper<detail::type_list<Ts...>> {
    typedef typed_actor<Ts...> return_type;
    template<typename... Vs>
    return_type operator()(Vs&&... vs) {
        auto iface = return_type::get_interface();
        auto tmp = remote_actor_impl(std::forward<Vs>(vs)..., std::move(iface));
        return actor_cast<return_type>(tmp);
    }
};

} // namespace io
} // namespace cppa

#endif // CPPA_IO_TYPED_REMOTE_ACTOR_HELPER_HPP
