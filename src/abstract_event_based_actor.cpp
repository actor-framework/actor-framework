#include "cppa/detail/invokable.hpp"
#include "cppa/detail/abstract_event_based_actor.hpp"

namespace cppa { namespace detail {

void abstract_event_based_actor::dequeue(invoke_rules&)
{
    quit(exit_reason::unallowed_function_call);
}

void abstract_event_based_actor::dequeue(timed_invoke_rules&)
{
    quit(exit_reason::unallowed_function_call);
}

void abstract_event_based_actor::handle_message(std::unique_ptr<queue_node>& node,
                                        invoke_rules& behavior)
{
    // no need to handle result
    (void) dq(node, behavior, m_buffer);
}

void abstract_event_based_actor::handle_message(std::unique_ptr<queue_node>& node,
                                        timed_invoke_rules& behavior)
{
    // request timeout only if we're running short on messages
    if (m_mailbox.empty() && has_pending_timeout() == false)
    {
        request_timeout(behavior.timeout());
    }
    switch (dq(node, behavior, m_buffer))
    {
        case dq_timeout_occured:
        {
            behavior.handle_timeout();
        }
        default: break;
    }
}

void abstract_event_based_actor::handle_message(std::unique_ptr<queue_node>& node)
{
    if (m_loop_stack.top().is_left())
    {
        handle_message(node, m_loop_stack.top().left());
    }
    else
    {
        handle_message(node, m_loop_stack.top().right());
    }
}

void abstract_event_based_actor::resume(util::fiber*, resume_callback* callback)
{
    auto done_cb = [&]()
    {
        m_state.store(scheduled_actor::done);
        while (!m_loop_stack.empty()) m_loop_stack.pop();
        callback->exec_done();
    };

    bool actor_done = false;
    std::unique_ptr<queue_node> node;
    do
    {
        if (m_loop_stack.empty())
        {
            cleanup(exit_reason::normal);
            done_cb();
            return;
        }
        else if (m_mailbox.empty())
        {
            m_state.store(scheduled_actor::about_to_block);
            CPPA_MEMORY_BARRIER();
            if (!m_mailbox.empty())
            {
                // someone preempt us
                m_state.store(scheduled_actor::ready);
            }
            else
            {
                // nothing to do
                return;
            }
        }
        node.reset(m_mailbox.pop());
        try
        {
            handle_message(node);
        }
        catch (actor_exited& what)
        {
            cleanup(what.reason());
            actor_done = true;
        }
        catch (...)
        {
            cleanup(exit_reason::unhandled_exception);
            actor_done = true;
        }
        if (actor_done)
        {
            done_cb();
            return;
        }
    }
    while (callback->still_ready());
}

} } // namespace cppa::detail
