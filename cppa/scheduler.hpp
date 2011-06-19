#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <memory>

#include "cppa/actor.hpp"
#include "cppa/attachable.hpp"
#include "cppa/scheduling_hint.hpp"

namespace cppa {

class context;
class actor_behavior;

/**
 * @brief
 */
class scheduler
{

 protected:

    /**
     * @brief Calls {@link context::exit(std::uint32_t) context::exit}.
     */
    void exit_context(context* ctx, std::uint32_t reason);

 public:

    virtual ~scheduler();

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
    virtual void register_converted_context(context* what) = 0;

    /**
     * @brief Informs the scheduler about a hidden (non-actor)
     *        context that should be counted by await_others_done().
     * @return An {@link attachable} that the hidden context has to destroy
     *         if his lifetime ends.
     */
    virtual attachable* register_hidden_context() = 0;

    /**
     * @brief Wait until all other actors finished execution.
     * @warning This function causes a deadlock if it's called from
     *          more than one actor.
     */
    virtual void await_others_done() = 0;

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
