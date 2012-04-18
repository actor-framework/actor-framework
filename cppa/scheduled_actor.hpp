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


#ifndef ACTOR_BEHAVIOR_HPP
#define ACTOR_BEHAVIOR_HPP

#include "cppa/config.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"

namespace cppa {

namespace util { class fiber; }

/**
 * @brief A base class for context-switching or thread-mapped actor
 *        implementations.
 *
 * This abstract class provides a class-based way to define context-switching
 * or thread-mapped actors. In general,
 * you always should use event-based actors. However, if you need to call
 * blocking functions, or need to have your own thread for other reasons,
 * this class can be used to define a class-based actor.
 */
class scheduled_actor : public local_actor
{

 public:

    scheduled_actor();

    scheduled_actor* next; // intrusive next pointer

    /**
     * @brief Can be overridden to perform cleanup code after an actor
     *        finished execution.
     * @warning Must not call any function manipulating the actor's state such
     *          as join, leave, link, or monitor.
     */
    virtual void on_exit();

    /**
     * @brief Can be overridden to initialize and actor before any
     *        message is handled.
     */
    virtual void init();

    // called from worker thread
    virtual void resume(util::fiber* from, scheduler::callback* cb) = 0;

    scheduled_actor* attach_to_scheduler(scheduler* sched);

 protected:

    scheduler* m_scheduler;

};

} // namespace cppa

#endif // ACTOR_BEHAVIOR_HPP
