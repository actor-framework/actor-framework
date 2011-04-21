#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "cppa/actor.hpp"
#include "cppa/context.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/scheduling_hint.hpp"

namespace cppa {

class scheduler
{

 public:

	virtual ~scheduler();

	/**
	 * @brief Spawn a new actor that executes <code>behavior->act()</code>
	 *        with the scheduling policy @p hint if possible.
	 */
	virtual actor_ptr spawn(actor_behavior* behavior,
							scheduling_hint hint) = 0;

	/**
	 * @brief Wait until all (other) actors finished execution.
	 */
	virtual void await_all_done() = 0;

};

scheduler* get_scheduler();

} // namespace cppa::detail

#endif // SCHEDULER_HPP
