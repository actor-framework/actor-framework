#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <chrono>
#include <memory>
#include <cstdint>

#include "cppa/atom.hpp"
#include "cppa/tuple.hpp"
#include "cppa/actor.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/attachable.hpp"
#include "cppa/scheduling_hint.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {

//class local_actor;
class actor_behavior;
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

    virtual ~scheduler();

    /**
     * @warning Always call scheduler::start on overriding.
     */
    virtual void start();

    /**
     * @warning Always call scheduler::stop on overriding.
     */
    virtual void stop();

    /**
     * @brief Spawns a new actor that executes <code>behavior->act()</code>
     *        with the scheduling policy @p hint if possible.
     */
    virtual actor_ptr spawn(actor_behavior* behavior,
                            scheduling_hint hint) = 0;

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
    void future_send(const actor_ptr& to,
                     const Duration& rel_time, const Data&... data)
    {
        static_assert(sizeof...(Data) > 0, "no message to send");
        any_tuple tup = make_tuple(util::duration(rel_time), to, data...);
        future_send_helper()->enqueue(self(), std::move(tup));
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
