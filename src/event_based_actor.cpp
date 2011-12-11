#include "cppa/event_based_actor.hpp"

namespace cppa {

void event_based_actor::on_exit()
{
}

void event_based_actor::unbecome()
{
    if (!m_loop_stack.empty()) m_loop_stack.pop();
}

void event_based_actor::become(invoke_rules&& behavior)
{
    reset_timeout();
    m_loop_stack.push(std::move(behavior));
}

void event_based_actor::become(timed_invoke_rules&& behavior)
{
    request_timeout(behavior.timeout());
    m_loop_stack.push(std::move(behavior));
}

} // namespace cppa
