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


#ifndef CPPA_ACTOR_REGISTRY_HPP
#define CPPA_ACTOR_REGISTRY_HPP

#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>
#include <condition_variable>

#include "cppa/actor.hpp"
#include "cppa/attachable.hpp"
#include "cppa/util/shared_spinlock.hpp"

namespace cppa { namespace detail {

class actor_registry {

 public:

    actor_registry();

    // return nullptr if the actor wasn't put *or* finished execution
    actor_ptr get(actor_id key) const;

    void put(actor_id key, const actor_ptr& value);

    void erase(actor_id key);

    // gets the next free actor id
    actor_id next_id();

    // increases running-actors-count by one
    void inc_running();

    // decreases running-actors-count by one
    void dec_running();

    size_t running() const;

    // blocks the caller until running-actors-count becomes @p expected
    void await_running_count_equal(size_t expected);

 private:

    std::atomic<size_t> m_running;
    std::atomic<std::uint32_t> m_ids;

    std::mutex m_running_mtx;
    std::condition_variable m_running_cv;

    mutable util::shared_spinlock m_instances_mtx;
    std::map<std::uint32_t, actor_ptr> m_instances;

};

} } // namespace cppa::detail

#endif // CPPA_ACTOR_REGISTRY_HPP
