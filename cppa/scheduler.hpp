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
#include "cppa/attachable.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/spawn_options.hpp"
#include "cppa/scheduled_actor.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {

class self_type;
class scheduler_helper;
namespace detail { class singleton_manager; } // namespace detail

namespace detail {
template<typename T>
struct is_self {
    typedef typename util::rm_ref<T>::type plain_type;
    static constexpr bool value = std::is_same<plain_type,self_type>::value;
};
template<typename T, typename U>
auto fwd(U& arg, typename std::enable_if<!is_self<T>::value>::type* = 0)
-> decltype(std::forward<T>(arg)) {
    return std::forward<T>(arg);
}
template<typename T, typename U>
local_actor* fwd(U& arg, typename std::enable_if<is_self<T>::value>::type* = 0){
    return arg;
}
} // namespace detail

/**
 * @brief This abstract class allows to create (spawn) new actors
 *        and offers delayed sends.
 */
class scheduler {

    scheduler_helper* m_helper;

    channel* delayed_send_helper();

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

 private:

    static scheduler* create_singleton();

    inline void dispose() { delete this; }

 public:

    typedef std::function<void(local_actor*)> init_callback;
    typedef std::function<void()>             void_function;

    const actor_ptr& printer() const;

    virtual void enqueue(scheduled_actor*) = 0;

    /**
     * @brief Informs the scheduler about a converted context
     *        (a thread that acts as actor).
     * @note Calls <tt>what->attach(...)</tt>.
     */
    virtual void register_converted_context(actor* what);

    /**
     * @brief Informs the scheduler about a hidden (non-actor)
     *        context that should be counted by await_others_done().
     * @returns An {@link attachable} that the hidden context has to destroy
     *         if his lifetime ends.
     */
    virtual attachable* register_hidden_context();

    template<typename Duration, typename... Data>
    void delayed_send(const channel_ptr& to,
                      const Duration& rel_time,
                      any_tuple data           ) {
        auto tup = make_any_tuple(atom("SEND"),
                                  util::duration{rel_time},
                                  to,
                                  std::move(data));
        delayed_send_helper()->enqueue(self, std::move(tup));
    }

    template<typename Duration, typename... Data>
    void delayed_reply(const actor_ptr& to,
                       const Duration& rel_time,
                       message_id_t id,
                       any_tuple data           ) {
        CPPA_REQUIRE(!id.valid() || id.is_response());
        if (id.valid()) {
            auto tup = make_any_tuple(atom("REPLY"),
                                      util::duration{rel_time},
                                      to,
                                      id,
                                      std::move(data));
            delayed_send_helper()->enqueue(self, std::move(tup));
        }
        else {
            this->delayed_send(to, rel_time, std::move(data));
        }
    }

    /**
     * @brief Executes @p ptr in this scheduler.
     */
    virtual actor_ptr exec(spawn_options opts, scheduled_actor_ptr ptr) = 0;

    /**
     * @brief Creates a new actor from @p actor_behavior and executes it
     *        in this scheduler.
     */
    virtual actor_ptr exec(spawn_options opts,
                           init_callback init_cb,
                           void_function actor_behavior) = 0;

    // hide implementation details for documentation
#   ifndef CPPA_DOCUMENTATION

    template<typename F, typename T0, typename... Ts>
    actor_ptr exec(spawn_options opts, init_callback cb,
                   F f, T0&& a0, Ts&&... as) {
        return this->exec(opts, cb, std::bind(f, detail::fwd<T0>(a0),
                                                 detail::fwd<Ts>(as)...));
    }

#   endif // CPPA_DOCUMENTATION

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

/**
 * @brief Returns the currently running scheduler.
 */
scheduler* get_scheduler();

} // namespace cppa

#endif // CPPA_SCHEDULER_HPP
