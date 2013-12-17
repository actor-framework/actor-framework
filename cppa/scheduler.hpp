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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef CPPA_SCHEDULER_HPP
#define CPPA_SCHEDULER_HPP

#include <chrono>
#include <memory>
#include <cstdint>
#include <functional>
#include <type_traits>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/channel.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/resumable.hpp"
#include "cppa/attachable.hpp"
#include "cppa/spawn_options.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {

class untyped_actor;
class scheduled_actor;
class scheduler_helper;
class untyped_actor;
typedef intrusive_ptr<scheduled_actor> scheduled_actor_ptr;
namespace detail { class singleton_manager; } // namespace detail

/**
 * @brief This abstract class allows to create (spawn) new actors
 *        and offers delayed sends.
 */
class scheduler {

    friend class detail::singleton_manager;

 protected:

    scheduler();

    virtual ~scheduler();


    /**
     * @warning Always call scheduler::initialize on overriding.
     */
    virtual void initialize();

    /**
     * @warning Always call scheduler::destroy on overriding.
     */
    virtual void destroy();

 public:

    actor printer() const;

    virtual void enqueue(resumable*) = 0;

    /**
     * @brief Informs the scheduler about a converted context
     *        (a thread that acts as actor).
     * @note Calls <tt>what->attach(...)</tt>.
     */
    virtual void register_converted_context(abstract_actor* what);

    /**
     * @brief Informs the scheduler about a hidden (non-actor)
     *        context that should be counted by await_others_done().
     * @returns An {@link attachable} that the hidden context has to destroy
     *         if his lifetime ends.
     */
    virtual attachable* register_hidden_context();

    template<typename Duration, typename... Data>
    void delayed_send(message_header hdr,
                      const Duration& rel_time,
                      any_tuple data           ) {
        auto tup = make_any_tuple(atom("SEND"),
                                  util::duration{rel_time},
                                  std::move(hdr),
                                  std::move(data));
        //TODO: delayed_send_helper()->enqueue(nullptr, std::move(tup));
    }

    template<typename Duration, typename... Data>
    void delayed_reply(message_header hdr,
                       const Duration& rel_time,
                       any_tuple data           ) {
        CPPA_REQUIRE(hdr.id.valid() && hdr.id.is_response());
        auto tup = make_any_tuple(atom("SEND"),
                                  util::duration{rel_time},
                                  std::move(hdr),
                                  std::move(data));
        //TODO: delayed_send_helper()->enqueue(nullptr, std::move(tup));
    }

 private:

    static scheduler* create_singleton();

    inline void dispose() { delete this; }

    actor delayed_send_helper();

    scheduler_helper* m_helper;

};

/**
 * @brief Sets the scheduler to @p sched.
 * @param sched A user-defined scheduler implementation.
 * @pre <tt>sched != nullptr</tt>.
 * @throws std::runtime_error if there's already a scheduler defined.
 */
void set_scheduler(scheduler* sched);

/**
 * @brief Sets a thread pool scheduler with @p num_threads worker threads.
 * @throws std::runtime_error if there's already a scheduler defined.
 */
void set_default_scheduler(size_t num_threads);

} // namespace cppa

#endif // CPPA_SCHEDULER_HPP
