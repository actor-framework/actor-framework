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


#ifndef CPPA_ACTOR_BEHAVIOR_HPP
#define CPPA_ACTOR_BEHAVIOR_HPP

#include "cppa/config.hpp"
#include "cppa/local_actor.hpp"

namespace cppa {

class scheduler;
namespace util { class fiber; }

enum class resume_result {
    actor_blocked,
    actor_done
};

enum scheduled_actor_type {
    context_switching_impl,
    event_based_impl
};

/**
 * @brief A base class for cooperatively scheduled actors.
 */
class scheduled_actor : public local_actor {

 public:

    scheduled_actor(bool enable_chained_send = false);

    /**
     * @brief Intrusive next pointer needed by the scheduler's job queue.
     */
    scheduled_actor* next;

    // called from worker thread
    virtual resume_result resume(util::fiber* from) = 0;

    void attach_to_scheduler(scheduler* sched);

    virtual bool has_behavior() = 0;

    virtual scheduled_actor_type impl_type() = 0;

 protected:

    scheduler* m_scheduler;

    bool initialized();

};

typedef intrusive_ptr<scheduled_actor> scheduled_actor_ptr;

} // namespace cppa

#endif // CPPA_ACTOR_BEHAVIOR_HPP
