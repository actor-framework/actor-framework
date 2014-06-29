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

#include "cppa/continue_helper.hpp"
#include "cppa/event_based_actor.hpp"

namespace cppa {

continue_helper::continue_helper(message_id mid, local_actor* self)
        : m_mid(mid), m_self(self) {}

continue_helper& continue_helper::continue_with(behavior::continuation_fun f) {
    auto ref_opt = m_self->sync_handler(m_mid);
    if (ref_opt) {
        behavior cpy = *ref_opt;
        *ref_opt = cpy.add_continuation(std::move(f));
    } else {
        CPPA_LOG_ERROR("failed to add continuation");
    }
    return *this;
}

} // namespace cppa
