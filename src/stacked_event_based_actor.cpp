#include "cppa/stacked_event_based_actor.hpp"

namespace cppa {

void stacked_event_based_actor::unbecome()
{
    if (!m_loop_stack.empty()) m_loop_stack.pop();
}

void stacked_event_based_actor::do_become(invoke_rules* behavior,
                                          bool has_ownership)
{
    reset_timeout();
    m_loop_stack.push(stack_element(behavior, has_ownership));
}

void stacked_event_based_actor::do_become(timed_invoke_rules* behavior,
                                          bool has_ownership)
{
    request_timeout(behavior->timeout());
    m_loop_stack.push(stack_element(behavior, has_ownership));
}

} // namespace cppa
