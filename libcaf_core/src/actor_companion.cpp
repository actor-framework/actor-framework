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

#include "caf/locks.hpp"
#include "caf/actor_companion.hpp"

namespace caf {

void actor_companion::disconnect(std::uint32_t rsn) {
    enqueue_handler tmp;
    { // lifetime scope of guard
        std::lock_guard<lock_type> guard(m_lock);
        m_on_enqueue.swap(tmp);
    }
    cleanup(rsn);
}

void actor_companion::on_enqueue(enqueue_handler handler) {
    std::lock_guard<lock_type> guard(m_lock);
    m_on_enqueue = std::move(handler);
}

void actor_companion::enqueue(const actor_addr& sender, message_id mid,
                              message content, execution_unit*) {
    message_pointer ptr;
    ptr.reset(detail::memory::create<mailbox_element>(sender, mid,
                                                      std::move(content)));
    shared_lock<lock_type> guard(m_lock);
    if (!m_on_enqueue) return;
    m_on_enqueue(std::move(ptr));
}

} // namespace caf
