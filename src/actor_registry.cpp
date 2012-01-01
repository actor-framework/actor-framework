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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
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


#include <mutex>
#include <limits>
#include <stdexcept>

#include "cppa/detail/actor_registry.hpp"
#include "cppa/util/shared_lock_guard.hpp"

namespace {

typedef cppa::detail::lock_guard<cppa::util::shared_spinlock> exclusive_guard;
typedef cppa::util::shared_lock_guard<cppa::util::shared_spinlock> shared_guard;

} // namespace <anonymous>

namespace cppa { namespace detail {

actor_registry::actor_registry() : m_running(0), m_ids(1)
{
}

void actor_registry::add(actor* whom)
{
    exclusive_guard guard(m_instances_mtx);
    m_instances.insert(std::make_pair(whom->id(), whom));
}

void actor_registry::remove(actor* whom)
{
    exclusive_guard guard(m_instances_mtx);
    m_instances.erase(whom->id());
}

actor_ptr actor_registry::find(actor_id whom)
{
    actor_ptr result;
    // lifetime scope of guard
    {
        shared_guard guard(m_instances_mtx);
        auto i = m_instances.find(whom);
        if (i != m_instances.end())
        {
            result.reset(i->second);
        }
    }
    return std::move(result);
}

std::uint32_t actor_registry::next_id()
{
    return m_ids.fetch_add(1);
}

void actor_registry::inc_running()
{
    ++m_running;
}

size_t actor_registry::running()
{
    return m_running.load();
}

void actor_registry::dec_running()
{
    size_t new_val = --m_running;
    if (new_val == std::numeric_limits<size_t>::max())
    {
        throw std::underflow_error("actor_count::dec()");
    }
    else if (new_val <= 1)
    {
        unique_lock<mutex> guard(m_running_mtx);
        m_running_cv.notify_all();
    }
}

void actor_registry::await_running_count_equal(size_t expected)
{
    unique_lock<mutex> guard(m_running_mtx);
    while (m_running.load() != expected)
    {
        m_running_cv.wait(guard);
    }
}

} } // namespace cppa::detail
