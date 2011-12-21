#include "cppa/event_based_actor.hpp"

namespace cppa {

void event_based_actor::clear()
{
    if (!m_loop_stack.empty()) m_loop_stack.pop();
}

void event_based_actor::do_become(behavior* bhvr)
{
    if (bhvr->is_left())
    {
        do_become(&(bhvr->left()), false);
    }
    else
    {
        do_become(&(bhvr->right()), false);
    }
}

void event_based_actor::do_become(invoke_rules* bhvr, bool has_ownership)
{
    clear();
    reset_timeout();
    m_loop_stack.push(stack_element(bhvr, has_ownership));
}

void event_based_actor::do_become(timed_invoke_rules* bhvr, bool has_ownership)
{
    clear();
    request_timeout(bhvr->timeout());
    m_loop_stack.push(stack_element(bhvr, has_ownership));
}

} // namespace cppa
