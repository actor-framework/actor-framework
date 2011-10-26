#ifndef MESSAGE_QUEUE_HPP
#define MESSAGE_QUEUE_HPP

#include "cppa/message.hpp"
#include "cppa/ref_counted.hpp"

namespace cppa {

// forward declaration
class invoke_rules;
class timed_invoke_rules;

/**
 * @brief A message queue with many-writers-single-reader semantics.
 */
class message_queue : public ref_counted
{

 protected:

    bool m_trap_exit;
    message m_last_dequeued;

 public:

    /**
     * @brief Creates an instance with <tt>{@link trap_exit()} == false</tt>.
     */
    message_queue();

    /**
     * @brief Enqueues a new element to the message queue.
     * @param msg The new message.
     */
    virtual void enqueue(const message& msg) = 0;

    /**
     * @brief Dequeues the oldest message (FIFO order) from the message queue.
     * @returns The oldest message from the queue.
     * @warning Call only from the owner of the queue.
     */
    virtual const message& dequeue() = 0;

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

    /**
     *
     * @warning Call only from the owner of the queue.
     */
    virtual bool try_dequeue(message&) = 0;

    /**
     *
     * @warning Call only from the owner of the queue.
     */
    virtual bool try_dequeue(invoke_rules&) = 0;

    /**
     *
     * @warning Call only from the owner of the queue.
     */
    inline bool trap_exit() const;

    /**
     *
     * @warning Call only from the owner of the queue.
     */
    inline void trap_exit(bool value);

    /**
     * @brief Gets the last dequeued message.
     * @returns The last message object that was removed from the queue
     *          by a dequeue() or try_dequeue() member function call.
     * @warning Call only from the owner of the queue.
     */
    inline const message& last_dequeued() const;

};

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

inline bool message_queue::trap_exit() const
{
    return m_trap_exit;
}

inline void message_queue::trap_exit(bool value)
{
    m_trap_exit = value;
}

inline const message& message_queue::last_dequeued() const
{
    return m_last_dequeued;
}

} // namespace cppa

#endif // MESSAGE_QUEUE_HPP
