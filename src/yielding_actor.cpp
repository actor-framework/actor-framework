#include "cppa/cppa.hpp"
#include "cppa/detail/intermediate.hpp"
#include "cppa/detail/yielding_actor.hpp"

namespace cppa { namespace detail {

yielding_actor::~yielding_actor()
{
    delete m_behavior;
}

void yielding_actor::run(void* ptr_arg)
{
    auto this_ptr = reinterpret_cast<yielding_actor*>(ptr_arg);
    auto behavior_ptr = this_ptr->m_behavior;
    if (behavior_ptr)
    {
        bool cleanup_called = false;
        try { behavior_ptr->act(); }
        catch (actor_exited&)
        {
            // cleanup already called by scheduled_actor::quit
            cleanup_called = true;
        }
        catch (...)
        {
            this_ptr->cleanup(exit_reason::unhandled_exception);
            cleanup_called = true;
        }
        if (!cleanup_called) this_ptr->cleanup(exit_reason::normal);
        try { behavior_ptr->on_exit(); }
        catch (...) { }
    }
    yield(yield_state::done);
}


void yielding_actor::yield_until_not_empty()
{
    while (m_mailbox.empty())
    {
        m_state.store(scheduled_actor::about_to_block);
        CPPA_MEMORY_BARRIER();
        // make sure mailbox is empty
        if (!m_mailbox.empty())
        {
            // someone preempt us
            //compare_exchange_state(scheduled_actor::about_to_block,
            //                       scheduled_actor::ready);
            m_state.store(scheduled_actor::ready);
            return;
        }
        else
        {
            yield(yield_state::blocked);
        }
    }
}

yielding_actor::filter_result yielding_actor::filter_msg(const any_tuple& msg)
{
    if (m_pattern(msg))
    {
        auto v0 = *reinterpret_cast<const atom_value*>(msg.at(0));
        auto v1 = *reinterpret_cast<const std::uint32_t*>(msg.at(1));
        if (v0 == atom(":Exit"))
        {
            if (m_trap_exit == false)
            {
                if (v1 != exit_reason::normal)
                {
                    quit(v1);
                }
                return normal_exit_signal;
            }
        }
        else if (v0 == atom(":Timeout"))
        {
            return (v1 == m_active_timeout_id) ? timeout_message
                                               : expired_timeout_message;
        }
    }
    return ordinary_message;
}

yielding_actor::dq_result yielding_actor::dq(std::unique_ptr<queue_node>& node,
                                             invoke_rules_base& rules,
                                             queue_node_buffer& buffer)
{
    switch (filter_msg(node->msg))
    {
        case normal_exit_signal:
        case expired_timeout_message:
        {
            // skip message
            return dq_indeterminate;
        }
        case timeout_message:
        {
            // m_active_timeout_id is already invalid
            m_has_pending_timeout_request = false;
            // restore mailbox before calling client
            if (!buffer.empty()) m_mailbox.push_front(std::move(buffer));
            return dq_timeout_occured;
        }
        default: break;
    }
    auto imd = rules.get_intermediate(node->msg);
    if (imd)
    {
        m_last_dequeued = std::move(node->msg);
        m_last_sender = std::move(node->sender);
        // restore mailbox before invoking imd
        if (!buffer.empty()) m_mailbox.push_front(std::move(buffer));
        // expire pending request
        if (m_has_pending_timeout_request)
        {
            ++m_active_timeout_id;
            m_has_pending_timeout_request = false;
        }
        imd->invoke();
        return dq_done;
    }
    else
    {
        buffer.push_back(node.release());
        return dq_indeterminate;
    }
}

void yielding_actor::dequeue(invoke_rules& rules)
{
    queue_node_buffer buffer;
    yield_until_not_empty();
    std::unique_ptr<queue_node> node(m_mailbox.pop());
    while (dq(node, rules, buffer) != dq_done)
    {
        yield_until_not_empty();
        node.reset(m_mailbox.pop());
    }
}

void yielding_actor::dequeue(timed_invoke_rules& rules)
{
    queue_node_buffer buffer;
    // try until a message was successfully dequeued
    for (;;)
    {
        if (m_mailbox.empty() && m_has_pending_timeout_request == false)
        {
            future_send(this,
                        rules.timeout(),
                        atom(":Timeout"),
                        ++m_active_timeout_id);
            m_has_pending_timeout_request = true;
        }
        yield_until_not_empty();
        std::unique_ptr<queue_node> node(m_mailbox.pop());
        switch (dq(node, rules, buffer))
        {
            case dq_done:
            {
                return;
            }
            case dq_timeout_occured:
            {
                rules.handle_timeout();
                return;
            }
            default: break;
        }
    }
}

void yielding_actor::resume(util::fiber* from, resume_callback* callback)
{
    set_self(this);
    for (;;)
    {
        call(&m_fiber, from);
        switch (yielded_state())
        {
            case yield_state::done:
            case yield_state::killed:
            {
                callback->exec_done();
                return;
            }
            case yield_state::ready:
            {
                if (callback->still_ready()) break;
                else return;
            }
            case yield_state::blocked:
            {
                switch (compare_exchange_state(scheduled_actor::about_to_block,
                                               scheduled_actor::blocked))
                {
                    case scheduled_actor::ready:
                    {
                        if (callback->still_ready()) break;
                        else return;
                    }
                    case scheduled_actor::blocked:
                    {
                        // wait until someone re-schedules that actor
                        return;
                    }
                    default:
                    {
                        // illegal state
                        exit(7);
                    }
                }
                break;
            }
            default:
            {
                // illegal state
                exit(8);
            }
        }
    }
}

} } // namespace cppa::detail
