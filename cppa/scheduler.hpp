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


#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <chrono>
#include <memory>
#include <cstdint>
#include <functional>

#include "cppa/self.hpp"
#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/channel.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/attachable.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/scheduling_hint.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {

class scheduled_actor;
class scheduler_helper;
/**
 * @brief
 */
class scheduler
{

    scheduler_helper* m_helper;

    channel* future_send_helper();

 protected:

    scheduler();

 public:

    struct callback
    {
        virtual ~callback();
        // called if an actor finished execution during resume()
        virtual void exec_done() = 0;
    };

    virtual ~scheduler();

    /**
     * @warning Always call scheduler::start on overriding.
     */
    virtual void start();

    /**
     * @warning Always call scheduler::stop on overriding.
     */
    virtual void stop();

    virtual void enqueue(scheduled_actor*) = 0;

    /**
     * @brief Spawns a new actor that executes <code>behavior->act()</code>
     *        with the scheduling policy @p hint if possible.
     */
    virtual actor_ptr spawn(std::function<void()> behavior,
                            scheduling_hint hint) = 0;

    /**
     * @brief Spawns a new event-based actor.
     */
    virtual actor_ptr spawn(scheduled_actor* what) = 0;

    /**
     * @brief Informs the scheduler about a converted context
     *        (a thread that acts as actor).
     * @note Calls <tt>what->attach(...)</tt>.
     */
    virtual void register_converted_context(local_actor* what);

    /**
     * @brief Informs the scheduler about a hidden (non-actor)
     *        context that should be counted by await_others_done().
     * @returns An {@link attachable} that the hidden context has to destroy
     *         if his lifetime ends.
     */
    virtual attachable* register_hidden_context();

    template<typename Duration, typename... Data>
    void future_send(actor_ptr const& to,
                     Duration const& rel_time, Data const&... data)
    {
        static_assert(sizeof...(Data) > 0, "no message to send");
        any_tuple data_tup = make_cow_tuple(data...);
        any_tuple tup = make_cow_tuple(util::duration(rel_time), to, data_tup);
        future_send_helper()->enqueue(self, std::move(tup));
    }

};

/**
 * @brief Sets the scheduler to @p sched.
 * @param sched A user-defined scheduler implementation.
 * @pre <tt>sched != nullptr</tt>.
 * @throws std::runtime_error if there's already a scheduler defined.
 */
void set_scheduler(scheduler* sched);

/**
 * @brief Gets the actual used scheduler implementation.
 * @returns The active scheduler (usually default constructed).
 */
scheduler* get_scheduler();

} // namespace cppa::detail

#endif // SCHEDULER_HPP
