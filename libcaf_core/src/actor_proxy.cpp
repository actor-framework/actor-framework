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

#include <utility>
#include <iostream>

#include "caf/locks.hpp"

#include "caf/atom.hpp"
#include "caf/to_string.hpp"
#include "caf/message.hpp"
#include "caf/scheduler.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/exit_reason.hpp"

#include "caf/detail/singletons.hpp"

using namespace std;

namespace caf {

actor_proxy::anchor::anchor(actor_proxy* instance) : m_ptr(instance) {}

actor_proxy::anchor::~anchor() {}

bool actor_proxy::anchor::expired() const { return !m_ptr; }

actor_proxy_ptr actor_proxy::anchor::get() {
    actor_proxy_ptr result;
    { // lifetime scope of guard
        shared_lock<detail::shared_spinlock> guard{m_lock};
        auto ptr = m_ptr.load();
        if (ptr) result.reset(ptr);
    }
    return result;
}

bool actor_proxy::anchor::try_expire() {
    std::lock_guard<detail::shared_spinlock> guard{m_lock};
    // double-check reference count
    if (m_ptr.load()->get_reference_count() == 0) {
        m_ptr = nullptr;
        return true;
    }
    return false;
}

actor_proxy::~actor_proxy() {}

actor_proxy::actor_proxy(actor_id aid, node_id nid)
        : super(aid, nid), m_anchor(new anchor{this}) {}

void actor_proxy::request_deletion() {
    if (m_anchor->try_expire()) delete this;
}

} // namespace caf
