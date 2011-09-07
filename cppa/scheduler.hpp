#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <chrono>
#include <memory>
#include <cstdint>

#include "cppa/atom.hpp"
#include "cppa/tuple.hpp"
#include "cppa/actor.hpp"
#include "cppa/attachable.hpp"
#include "cppa/scheduling_hint.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {

// forward declarations

class context;
class actor_behavior;
class scheduler_helper;

context* self();

/**
 * @brief
 */
class scheduler
{

    scheduler_helper* m_helper;

    channel* future_send_helper();

 protected:

    scheduler();

    /**
     * @brief Calls {@link context::exit(std::uint32_t) context::exit}.
     */
    void exit_context(context* ctx, std::uint32_t reason);

 public:

    virtual ~scheduler();

    /**
     * @warning Always call super::start on overriding.
     */
    virtual void start();

    /**
     * @warning Always call super::stop on overriding.
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
    virtual void register_converted_context(context* what);

    /**
     * @brief Informs the scheduler about a hidden (non-actor)
     *        context that should be counted by await_others_done().
     * @return An {@link attachable} that the hidden context has to destroy
     *         if his lifetime ends.
     */
    virtual attachable* register_hidden_context();

    /**
     * @brief Wait until all other actors finished execution.
     * @warning This function causes a deadlock if it's called from
     *          more than one actor.
     */
    virtual void await_others_done();

    template<typename Duration, typename... Data>
    void future_send(actor_ptr whom, const Duration& d, const Data&... data)
    {
        static_assert(sizeof...(Data) > 0, "no message to send");
        any_tuple tup = make_tuple(util::duration(d), data...);
        future_send_helper()->enqueue(message(whom, whom, tup));
    }

};

/**
 * @brief Sets the scheduler to @p sched;
 * @throws std::runtime_error if there's already a scheduler defined.
 */
void set_scheduler(scheduler* sched);

/**
 * @brief Gets the actual used scheduler implementation.
 * @return The active scheduler (default constructed).
 */
scheduler* get_scheduler();

} // namespace cppa::detail

#endif // SCHEDULER_HPP
