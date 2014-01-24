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


#ifndef CPPA_RAW_ACCESS_HPP
#define CPPA_RAW_ACCESS_HPP

#include "cppa/actor.hpp"
#include "cppa/channel.hpp"
#include "cppa/actor_addr.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/abstract_channel.hpp"

namespace cppa { namespace detail {

class raw_access {

 public:

    static abstract_actor* get(const actor& hdl) {
        return hdl.m_ops.m_ptr.get();
    }

    static abstract_actor* get(const actor_addr& hdl) {
        return hdl.m_ops.m_ptr.get();
    }

    static abstract_channel* get(const channel& hdl) {
        return hdl.m_ptr.get();
    }

    static actor unsafe_cast(abstract_actor* ptr) {
        return {ptr};
    }

    static actor unsafe_cast(const actor_addr& hdl) {
        return {get(hdl)};
    }

};

} } // namespace cppa::detail

#endif // CPPA_RAW_ACCESS_HPP
