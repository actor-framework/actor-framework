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


#ifndef CPPA_DETAIL_RAW_ACCESS_HPP
#define CPPA_DETAIL_RAW_ACCESS_HPP

#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/actor_addr.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/abstract_group.hpp"
#include "cppa/abstract_channel.hpp"

namespace cppa {
namespace detail {

class raw_access {

 public:

    template<typename ActorHandle>
    static abstract_actor* get(const ActorHandle& hdl) {
        return hdl.m_ptr.get();
    }

    static abstract_channel* get(const channel& hdl) {
        return hdl.m_ptr.get();
    }

    static abstract_group* get(const group& hdl) {
        return hdl.m_ptr.get();
    }

    static actor unsafe_cast(abstract_actor* ptr) {
        return {ptr};
    }

    static actor unsafe_cast(const actor_addr& hdl) {
        return {get(hdl)};
    }

    static actor unsafe_cast(const abstract_actor_ptr& ptr) {
        return {ptr.get()};
    }

    template<typename T>
    static void unsafe_assign(T& lhs, const actor& rhs) {
        lhs = T{get(rhs)};
    }

    template<typename T>
    static void unsafe_assign(T& lhs, const abstract_actor_ptr& ptr) {
        lhs = T{ptr.get()};
    }

};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_RAW_ACCESS_HPP
