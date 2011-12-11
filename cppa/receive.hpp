#ifndef RECEIVE_HPP
#define RECEIVE_HPP

#include "cppa/local_actor.hpp"

namespace cppa {

/**
 * @brief Dequeues the next message from the mailbox that's matched
 *        by @p rules and executes the corresponding callback.
 */
inline void receive(invoke_rules& rules)
{
    self()->dequeue(rules);
}

inline void receive(timed_invoke_rules& rules)
{
    self()->dequeue(rules);
}

inline void receive(timed_invoke_rules&& rules)
{
    timed_invoke_rules tmp(std::move(rules));
    self()->dequeue(tmp);
}

inline void receive(invoke_rules&& rules)
{
    invoke_rules tmp(std::move(rules));
    self()->dequeue(tmp);
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

} // namespace cppa

#endif // RECEIVE_HPP
