#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "cppa/actor.hpp"
#include "cppa/message_queue.hpp"

namespace cppa {

class scheduler;

/**
 *
 */
class context : public actor
{

    friend class scheduler;

 public:

    /**
     * @brief Cause this context to send an exit signal to all of its linked
     *        linked actors and set its state to @c exited.
     */
    virtual void quit(std::uint32_t reason) = 0;

    /**
     * @brief
     */
    virtual message_queue& mailbox() = 0;

    /**
     * @brief Default implementation of
     *        {@link channel::enqueue(const message&)}.
     *
     * Calls <code>mailbox().enqueue(msg)</code>.
     */
    virtual void enqueue /*[[override]]*/ (const message& msg);

    void trap_exit(bool new_value);

};

/**
 * @brief Get a pointer to the current active context.
 */
context* self();

// "private" function
void set_self(context*);

// "private" function, returns active context (without creating it if needed)
context* unchecked_self();

} // namespace cppa

#endif // CONTEXT_HPP
