#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "cppa/actor.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/util/single_reader_queue.hpp"

namespace cppa {

class scheduler;

/**
 * @brief Base class for local running Actors.
 */
class local_actor : public actor
{

    friend class scheduler;

 protected:

    bool m_trap_exit;
    actor_ptr m_last_sender;
    any_tuple m_last_dequeued;

 public:

    local_actor();

    /**
     * @brief Finishes execution of this actor.
     *
     * Causes this actor to send an exit signal to all of its
     * linked actors, sets its state to @c exited and throws
     * {@link actor_exited} to cleanup the stack.
     * @param reason Exit reason that will be send to linked actors.
     * @throws actor_exited
     */
    virtual void quit(std::uint32_t reason) = 0;

    /**
     * @brief Removes the first element from the queue that is matched
     *        by @p rules and invokes the corresponding callback.
     * @param rules
     * @warning Call only from the owner of the queue.
     */
    virtual void dequeue(invoke_rules& rules) = 0;

    /**
     * @brief
     * @param rules
     * @warning Call only from the owner of the queue.
     */
    virtual void dequeue(timed_invoke_rules& rules) = 0;

    inline bool trap_exit() const;

    inline void trap_exit(bool new_value);

    inline any_tuple& last_dequeued();

    inline actor_ptr& last_sender();

};

inline bool local_actor::trap_exit() const
{
    return m_trap_exit;
}

inline void local_actor::trap_exit(bool new_value)
{
    m_trap_exit = new_value;
}

inline any_tuple& local_actor::last_dequeued()
{
    return m_last_dequeued;
}

inline actor_ptr& local_actor::last_sender()
{
    return m_last_sender;
}


/**
 * @brief Get a pointer to the current active context.
 * @returns A pointer that identifies the calling actor.
 */
local_actor* self();

// "private" function
void set_self(local_actor*);

// "private" function, returns active context (without creating it if needed)
local_actor* unchecked_self();

} // namespace cppa

#endif // CONTEXT_HPP
