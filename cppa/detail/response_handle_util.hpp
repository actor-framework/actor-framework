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

#ifndef CPPA_DETAIL_RESPONSE_FUTURE_DETAIL_HPP
#define CPPA_DETAIL_RESPONSE_FUTURE_DETAIL_HPP

#include "cppa/on.hpp"
#include "cppa/skip_message.hpp"
#include "cppa/system_messages.hpp"

#include "cppa/detail/type_traits.hpp"

namespace cppa {
namespace detail {

template<typename Actor, typename... Fs>
behavior fs2bhvr(Actor* self, Fs... fs) {
    auto handle_sync_timeout = [self]() -> skip_message_t {
        self->handle_sync_timeout();
        return {};
    };
    return behavior{
        on<sync_timeout_msg>() >> handle_sync_timeout,
        on<unit_t>() >> skip_message,
        on<sync_exited_msg>() >> skip_message,
        (on(any_vals, arg_match) >> std::move(fs))...
    };
}

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_RESPONSE_FUTURE_DETAIL_HPP
