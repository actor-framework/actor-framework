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

#include "caf/actor.hpp"
#include "caf/group.hpp"
#include "caf/channel.hpp"
#include "caf/message.hpp"
#include "caf/actor_cast.hpp"

namespace caf {

channel::channel(const actor& other)
        : m_ptr(actor_cast<abstract_channel_ptr>(other)) {}

channel::channel(const group& other)
        : m_ptr(actor_cast<abstract_channel_ptr>(other)) {}

channel::channel(const invalid_actor_t&) : m_ptr(nullptr) {}

channel::channel(const invalid_group_t&) : m_ptr(nullptr) {}

intptr_t channel::compare(const abstract_channel* lhs,
                          const abstract_channel* rhs) {
    return reinterpret_cast<intptr_t>(lhs) - reinterpret_cast<intptr_t>(rhs);
}

channel::channel(abstract_channel* ptr) : m_ptr(ptr) {}

intptr_t channel::compare(const channel& other) const {
    return compare(m_ptr.get(), other.m_ptr.get());
}

intptr_t channel::compare(const actor& other) const {
    return compare(m_ptr.get(), actor_cast<abstract_actor_ptr>(other).get());
}

intptr_t channel::compare(const abstract_channel* other) const {
    return compare(m_ptr.get(), other);
}

} // namespace caf
