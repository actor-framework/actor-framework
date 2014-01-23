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


#ifndef CPPA_DETAIL_RESPONSE_FUTURE_UTIL_HPP
#define CPPA_DETAIL_RESPONSE_FUTURE_UTIL_HPP

#include "cppa/on.hpp"
#include "cppa/match_hint.hpp"
#include "cppa/system_messages.hpp"

#include "cppa/util/type_traits.hpp"

namespace cppa {
namespace detail {

template<typename Actor, typename... Fs>
behavior fs2bhvr(Actor* self, Fs... fs) {
    auto handle_sync_timeout = [self]() -> match_hint {
        self->handle_sync_timeout();
        return match_hint::skip;
    };
    return behavior{
        on<sync_timeout_msg>() >> handle_sync_timeout,
        on(atom("VOID")) >> skip_message,
        on(atom("EXITED")) >> skip_message,
        (on(any_vals, arg_match) >> std::move(fs))...
    };
}

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_RESPONSE_FUTURE_UTIL_HPP
