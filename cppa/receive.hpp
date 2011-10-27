#ifndef RECEIVE_HPP
#define RECEIVE_HPP

#include "cppa/local_actor.hpp"

namespace cppa {

/**
 * @brief Dequeues the next message from the mailbox.
 */
inline const message& receive()
{
    return self()->mailbox().dequeue();
}

/**
 * @brief Dequeues the next message from the mailbox that's matched
 *        by @p rules and executes the corresponding callback.
 */
inline void receive(invoke_rules& rules)
{
    self()->mailbox().dequeue(rules);
}

inline void receive(timed_invoke_rules& rules)
{
    self()->mailbox().dequeue(rules);
}

inline void receive(timed_invoke_rules&& rules)
{
    timed_invoke_rules tmp(std::move(rules));
    self()->mailbox().dequeue(tmp);
}

inline void receive(invoke_rules&& rules)
{
    invoke_rules tmp(std::move(rules));
    self()->mailbox().dequeue(tmp);
}

template<typename Head, typename... Tail>
void receive(invoke_rules&& rules, Head&& head, Tail&&... tail)
{
    invoke_rules tmp(std::move(rules));
    receive(tmp.splice(std::forward<Head>(head)),
            std::forward<Tail>(tail)...);
}

template<typename Head, typename... Tail>
void receive(invoke_rules& rules, Head&& head, Tail&&... tail)
{
    receive(rules.splice(std::forward<Head>(head)),
            std::forward<Tail>(tail)...);
}

/**
 * @brief Tries to dequeue the next message from the mailbox.
 * @returns @p true if a messages was dequeued;
 *         @p false if the mailbox is empty
 */
inline bool try_receive(message& msg)
{
    return self()->mailbox().try_dequeue(msg);
}

/**
 * @brief Tries to dequeue the next message from the mailbox.
 * @returns @p true if a messages was dequeued;
 *         @p false if the mailbox is empty
 */
inline bool try_receive(invoke_rules& rules)
{
    return self()->mailbox().try_dequeue(rules);
}

} // namespace cppa

#endif // RECEIVE_HPP
