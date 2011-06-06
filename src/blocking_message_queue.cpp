#include <vector>

#include "cppa/atom.hpp"
#include "cppa/match.hpp"
#include "cppa/context.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/invoke_rules.hpp"

#include "cppa/util/singly_linked_list.hpp"

#include "cppa/detail/intermediate.hpp"
#include "cppa/detail/blocking_message_queue.hpp"

namespace cppa { namespace {

enum throw_on_exit_result
{
    not_an_exit_signal,
    normal_exit_signal
};

throw_on_exit_result throw_on_exit(const message& msg)
{
    if (match<atom(":Exit"), std::uint32_t>(msg.content()))
    {
        auto reason = *reinterpret_cast<const std::uint32_t*>(msg.content().at(1));
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

blocking_message_queue::blocking_message_queue() : m_trap_exit(false)
{
}

void blocking_message_queue::trap_exit(bool new_value)
{
    m_trap_exit = new_value;
}

void blocking_message_queue::enqueue(const message& msg)
{
    m_queue.push_back(new queue_node(msg));
}

const message& blocking_message_queue::dequeue()
{
    queue_node* msg = m_queue.pop();
    m_last_dequeued = std::move(msg->msg);
    delete msg;
    if (!m_trap_exit)
    {
        if (throw_on_exit(m_last_dequeued) == normal_exit_signal)
        {
            // exit_reason::normal is ignored by default,
            // dequeue next message
            return dequeue();
        }
    }
    return m_last_dequeued;
}

void blocking_message_queue::dequeue(invoke_rules& rules)
{
    queue_node* amsg = nullptr;//= m_queue.pop();
    util::singly_linked_list<queue_node> buffer;
    intrusive_ptr<detail::intermediate> imd;
    bool done = false;
    do
    {
        amsg = m_queue.pop();
        if (!m_trap_exit)
        {
            if (throw_on_exit(amsg->msg) == normal_exit_signal)
            {
                // ignored by default
                delete amsg;
                amsg = nullptr;
            }
        }
        if (amsg)
        {
            imd = rules.get_intermediate(amsg->msg.content());
            if (imd)
            {
                done = true;
            }
            else
            {
                buffer.push_back(amsg);
            }
        }
    }
    while (!done);
    m_last_dequeued = amsg->msg;
    if (!buffer.empty()) m_queue.push_front(std::move(buffer));
    imd->invoke();
    delete amsg;
}

bool blocking_message_queue::try_dequeue(message& msg)
{
    if (!m_queue.empty())
    {
        msg = dequeue();
        return true;
    }
    return false;
}

bool blocking_message_queue::try_dequeue(invoke_rules& rules)
{
    if (!m_queue.empty())
    {
        dequeue(rules);
        return true;
    }
    return false;
}

const message& blocking_message_queue::last_dequeued()
{
    return m_last_dequeued;
}

} } // namespace hamcast::detail
