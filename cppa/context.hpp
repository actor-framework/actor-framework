#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "cppa/actor.hpp"
#include "cppa/message_queue.hpp"

namespace cppa {

class context : public actor
{

 public:

	virtual message_queue& mailbox() = 0;

	/**
	 * @brief Default implementation of
	 *        {@link channel::enqueue(const message&)}.
	 *
	 * Calls <code>mailbox().enqueue(msg)</code>.
	 */
	virtual void enqueue /*[[override]]*/ (const message& msg);

};

/**
 * @brief Get a pointer to the current active context.
 */
context* self();

// "private" function
void set_self(context*);

} // namespace cppa

#endif // CONTEXT_HPP
