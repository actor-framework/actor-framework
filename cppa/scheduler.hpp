#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "cppa/actor.hpp"
#include "cppa/scheduling_hint.hpp"

namespace cppa {

class context;
class actor_behavior;

class scheduler
{

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
     */
    virtual void register_converted_context(context* what) = 0;

    /**
     * @brief Informs the scheduler that the convertex context @p what
     *        finished execution.
     */
    virtual void unregister_converted_context(context* what) = 0;

    /**
     * @brief Wait until all other actors finished execution.
     * @warning This function causes a deadlock if it's called from
     *          more than one actor.
     */
    virtual void await_others_done() = 0;

};

scheduler* get_scheduler();

} // namespace cppa::detail

#endif // SCHEDULER_HPP
