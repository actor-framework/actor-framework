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


#ifndef ACTOR_REGISTRY_HPP
#define ACTOR_REGISTRY_HPP

#include <map>
#include <atomic>
#include <cstdint>

#include "cppa/actor.hpp"
#include "cppa/detail/thread.hpp"
#include "cppa/util/shared_spinlock.hpp"

namespace cppa { namespace detail {

class actor_registry
{

 public:

    actor_registry();

    // adds @p whom to the list of known actors
    void add(actor* whom);

    // removes @p whom from the list of known actors
    void remove(actor* whom);

    // looks for an actor with id @p whom in the list of known actors
    actor_ptr find(actor_id whom);

    // gets the next free actor id
    actor_id next_id();

    // increases running-actors-count by one
    void inc_running();

    // decreases running-actors-count by one
    void dec_running();

    size_t running();

    // blocks the caller until running-actors-count becomes @p expected
    void await_running_count_equal(size_t expected);

 private:

    std::atomic<size_t> m_running;
    std::atomic<std::uint32_t> m_ids;

    mutex m_running_mtx;
    condition_variable m_running_cv;

    util::shared_spinlock m_instances_mtx;
    std::map<std::uint32_t, actor*> m_instances;

};

} } // namespace cppa::detail

#endif // ACTOR_REGISTRY_HPP
