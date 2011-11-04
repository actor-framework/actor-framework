#include <vector>
#include <memory>

#include "cppa/atom.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/invoke_rules.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/singly_linked_list.hpp"

#include "cppa/detail/intermediate.hpp"
#include "cppa/detail/blocking_message_queue.hpp"

namespace cppa { namespace {

enum throw_on_exit_result
{
    not_an_exit_signal,
    normal_exit_signal
};

template<class Pattern>
throw_on_exit_result throw_on_exit(Pattern& exit_pattern, const any_tuple& msg)
{
    if (exit_pattern(msg))
    {
        auto reason = *reinterpret_cast<const std::uint32_t*>(msg.at(2));
        if (reason != exit_reason::normal)
        {
            // throws
            cppa::self()->quit(reason);
        }
        else
        {
            return normal_exit_signal;
        }
    }
    return not_an_exit_signal;
}

} } // namespace cppa::<anonymous>

namespace cppa { namespace detail {

blocking_message_queue_impl::queue_node::queue_node(any_tuple&& content)
    : next(nullptr), msg(std::move(content))
{
}

blocking_message_queue_impl::queue_node::queue_node(const any_tuple& content)
    : next(nullptr), msg(content)
{
}

blocking_message_queue_impl::blocking_message_queue_impl()
    : m_exit_msg_pattern(atom("Exit"))
{
}

void blocking_message_queue_impl::enqueue(any_tuple&& msg)
{
    m_queue.push_back(new queue_node(std::move(msg)));
}

void blocking_message_queue_impl::enqueue(const any_tuple& msg)
{
    m_queue.push_back(new queue_node(msg));
}

bool blocking_message_queue_impl::dequeue_impl(any_tuple& storage)
{
    std::unique_ptr<queue_node> node(m_queue.pop());
    if (!m_trap_exit)
    {
        if (throw_on_exit(m_exit_msg_pattern, node->msg) == normal_exit_signal)
        {
            // exit_reason::normal is ignored by default,
            // dequeue next message
            return false;
        }
    }
    storage = std::move(node->msg);
    return true;
}

bool blocking_message_queue_impl::dq(std::unique_ptr<queue_node>& node,
                                     invoke_rules_base& rules,
                                     queue_node_buffer& buffer)
{
    if (   m_trap_exit == false
        && throw_on_exit(m_exit_msg_pattern, node->msg) == normal_exit_signal)
    {
        return false;
    }
    auto imd = rules.get_intermediate(node->msg);
    if (imd)
    {
        m_last_dequeued = node->msg;
        // restore mailbox before invoking imd
        if (!buffer.empty()) m_queue.push_front(std::move(buffer));
        imd->invoke();
        return true;
    }
    else
    {
        buffer.push_back(node.release());
        return false;
    }
}

bool blocking_message_queue_impl::dequeue_impl(timed_invoke_rules& rules,
                                               queue_node_buffer& buffer)
{
    std::unique_ptr<queue_node> node(m_queue.try_pop());
    if (!node)
    {
        auto timeout = now();
        timeout += rules.timeout();
        node.reset(m_queue.try_pop(timeout));
        if (!node)
        {
            if (!buffer.empty()) m_queue.push_front(std::move(buffer));
            rules.handle_timeout();
            return true;
        }
    }
    return dq(node, rules, buffer);
}

bool blocking_message_queue_impl::dequeue_impl(invoke_rules& rules,
                                               queue_node_buffer& buffer)
{
    std::unique_ptr<queue_node> node(m_queue.pop());
    return dq(node, rules, buffer);
}

} } // namespace hamcast::detail
