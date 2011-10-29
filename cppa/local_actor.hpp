#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "cppa/actor.hpp"
#include "cppa/message_queue.hpp"

namespace cppa {

class scheduler;

/**
 * @brief Base class for local running Actors.
 */
class local_actor : public actor
{

    friend class scheduler;

 public:

    /**
     * @brief Finishes execution of this actor.
     *
     * Cause this actor to send an exit signal to all of its
     * linked actors, sets its state to @c exited and throws
     * {@link actor_exited} to cleanup the stack.
     * @throws actor_exited
     */
    virtual void quit(std::uint32_t reason) = 0;

    /**
     * @brief
     */
    virtual message_queue& mailbox() = 0;

    virtual const message_queue& mailbox() const = 0;

    /**
     * @brief Default implementation of
     *        {@link channel::enqueue(const message&)}.
     *
     * Calls <code>mailbox().enqueue(msg)</code>.
     */
    virtual void enqueue(const any_tuple& msg) /*override*/;

    inline bool trap_exit() const;

    inline void trap_exit(bool new_value);

};

inline bool local_actor::trap_exit() const
{
    return mailbox().trap_exit();
}

inline void local_actor::trap_exit(bool new_value)
{
    mailbox().trap_exit(new_value);
}

/**
 * @brief Get a pointer to the current active context.
 */
local_actor* self();

// "private" function
void set_self(local_actor*);

// "private" function, returns active context (without creating it if needed)
local_actor* unchecked_self();

} // namespace cppa

#endif // CONTEXT_HPP
