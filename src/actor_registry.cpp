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

#include <mutex>
#include <limits>
#include <stdexcept>

#include "cppa/attachable.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/detail/actor_registry.hpp"

#include "cppa/locks.hpp"
#include "cppa/detail/logging.hpp"
#include "cppa/detail/shared_spinlock.hpp"

namespace cppa {
namespace detail {

namespace {

using exclusive_guard = unique_lock<shared_spinlock>;
using shared_guard = shared_lock<shared_spinlock>;

} // namespace <anonymous>

actor_registry::~actor_registry() {
    // nop
}

actor_registry::actor_registry() : m_running(0), m_ids(1) {
    // nop
}

actor_registry::value_type actor_registry::get_entry(actor_id key) const {
    shared_guard guard(m_instances_mtx);
    auto i = m_entries.find(key);
    if (i != m_entries.end()) {
        return i->second;
    }
    CPPA_LOG_DEBUG("key not found, assume the actor no longer exists: " << key);
    return {nullptr, exit_reason::unknown};
}

void actor_registry::put(actor_id key, const abstract_actor_ptr& val) {
    if (val == nullptr) {
        return;
    }
    auto entry = std::make_pair(key, value_type(val, exit_reason::not_exited));
    { // lifetime scope of guard
        exclusive_guard guard(m_instances_mtx);
        if (!m_entries.insert(entry).second) {
            // already defined
            return;
        }
    }
    // attach functor without lock
    CPPA_LOG_INFO("added actor with ID " << key);
    actor_registry* reg = this;
    val->attach_functor([key, reg](uint32_t reason) {
        reg->erase(key, reason);
    });
}

void actor_registry::erase(actor_id key, uint32_t reason) {
    exclusive_guard guard(m_instances_mtx);
    auto i = m_entries.find(key);
    if (i != m_entries.end()) {
        auto& entry = i->second;
        CPPA_LOG_INFO("erased actor with ID " << key << ", reason " << reason);
        entry.first = nullptr;
        entry.second = reason;
    }
}

uint32_t actor_registry::next_id() {
    return ++m_ids;
}

void actor_registry::inc_running() {
#   if CPPA_LOG_LEVEL >= CPPA_DEBUG
        CPPA_LOG_DEBUG("new value = " << ++m_running);
#   else
        ++m_running;
#   endif
}

size_t actor_registry::running() const {
    return m_running.load();
}

void actor_registry::dec_running() {
    size_t new_val = --m_running;
    if (new_val <= 1) {
        std::unique_lock<std::mutex> guard(m_running_mtx);
        m_running_cv.notify_all();
    }
    CPPA_LOG_DEBUG(CPPA_ARG(new_val));
}

void actor_registry::await_running_count_equal(size_t expected) {
    CPPA_REQUIRE(expected == 0 || expected == 1);
    CPPA_LOG_TRACE(CPPA_ARG(expected));
    std::unique_lock<std::mutex> guard{m_running_mtx};
    while (m_running != expected) {
        CPPA_LOG_DEBUG("count = " << m_running.load());
        m_running_cv.wait(guard);
    }
}

} // namespace detail
} // namespace cppa
