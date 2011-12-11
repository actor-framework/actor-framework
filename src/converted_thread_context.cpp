#include <memory>
#include <algorithm>

#include "cppa/exception.hpp"
#include "cppa/detail/invokable.hpp"
#include "cppa/detail/intermediate.hpp"
#include "cppa/detail/converted_thread_context.hpp"

namespace cppa { namespace detail {

converted_thread_context::converted_thread_context()
    : m_exit_msg_pattern(atom(":Exit"))
{
}

void converted_thread_context::quit(std::uint32_t reason)
{
    try { super::cleanup(reason); } catch(...) { }
    // actor_exited should not be catched, but if anyone does,
    // the next call to self() must return a newly created instance
    set_self(nullptr);
    throw actor_exited(reason);
}

void converted_thread_context::cleanup(std::uint32_t reason)
{
    super::cleanup(reason);
}

void converted_thread_context::enqueue(actor* sender, any_tuple&& msg)
{
    m_mailbox.push_back(new queue_node(sender, std::move(msg)));
}

void converted_thread_context::enqueue(actor* sender, const any_tuple& msg)
{
    m_mailbox.push_back(new queue_node(sender, msg));
}

void converted_thread_context::dequeue(invoke_rules& rules) /*override*/
{
    queue_node_buffer buffer;
    std::unique_ptr<queue_node> node(m_mailbox.pop());
    while (dq(node, rules, buffer) == false)
    {
        node.reset(m_mailbox.pop());
    }
}

void converted_thread_context::dequeue(timed_invoke_rules& rules)  /*override*/
{
    auto timeout = now();
    timeout += rules.timeout();
    queue_node_buffer buffer;
    std::unique_ptr<queue_node> node(m_mailbox.try_pop());
    do
    {
        while (!node)
        {
            node.reset(m_mailbox.try_pop(timeout));
            if (!node)
            {
                if (!buffer.empty()) m_mailbox.push_front(std::move(buffer));
                rules.handle_timeout();
                return;
            }
        }
    }
    while (dq(node, rules, buffer) == false);
}
converted_thread_context::throw_on_exit_result
converted_thread_context::throw_on_exit(const any_tuple& msg)
{
    if (m_exit_msg_pattern(msg))
    {
        auto reason = *reinterpret_cast<const std::uint32_t*>(msg.at(2));
        if (reason != exit_reason::normal)
        {
            // throws
            quit(reason);
        }
        else
        {
            return normal_exit_signal;
        }
    }
    return not_an_exit_signal;
}

bool converted_thread_context::dq(std::unique_ptr<queue_node>& node,
                                  invoke_rules_base& rules,
                                  queue_node_buffer& buffer)
{
    if (m_trap_exit == false && throw_on_exit(node->msg) == normal_exit_signal)
    {
        return false;
    }
    auto imd = rules.get_intermediate(node->msg);
    if (imd)
    {
        m_last_dequeued = std::move(node->msg);
        m_last_sender = std::move(node->sender);
        // restore mailbox before invoking imd
        if (!buffer.empty()) m_mailbox.push_front(std::move(buffer));
        imd->invoke();
        return true;
    }
    else
    {
        buffer.push_back(node.release());
        return false;
    }
}

} } // namespace cppa::detail
