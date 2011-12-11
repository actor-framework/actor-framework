#include "cppa/event_based_actor.hpp"

namespace cppa {

void event_based_actor::clear()
{
    if (!m_loop_stack.empty()) m_loop_stack.pop();
}

void event_based_actor::do_become(invoke_rules* behavior,
                                  bool has_ownership)
{
    clear();
    reset_timeout();
    m_loop_stack.push(stack_element(behavior, has_ownership));
}

void event_based_actor::do_become(timed_invoke_rules* behavior,
                                  bool has_ownership)
{
    clear();
    request_timeout(behavior->timeout());
    m_loop_stack.push(stack_element(behavior, has_ownership));
}

} // namespace cppa
