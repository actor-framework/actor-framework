#include "cppa/config.hpp"

#include <mutex>
#include <atomic>
#include <iostream>

#include "cppa/match.hpp"
#include "cppa/context.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/detail/intermediate.hpp"
#include "cppa/detail/scheduled_actor.hpp"
#include "cppa/detail/yield_interface.hpp"
#include "cppa/detail/yielding_message_queue.hpp"

using std::cout;
using std::cerr;
using std::endl;

namespace {

typedef std::lock_guard<cppa::util::shared_spinlock> exclusive_guard;
typedef cppa::util::shared_lock_guard<cppa::util::shared_spinlock> shared_guard;

enum yield_on_exit_result
{
    normal_exit_signal,
    not_an_exit_signal
};

yield_on_exit_result yield_on_exit_msg(const cppa::message& msg)
{
    if (cppa::match<cppa::atom_value, std::uint32_t>(msg.content(), nullptr,
                                                     cppa::atom(":Exit")))
    {
        auto reason = *reinterpret_cast<const std::uint32_t*>(msg.content().at(1));
        if (reason != cppa::exit_reason::normal)
        {
            // yields
            cppa::self()->quit(reason);
        }
        else
        {
            return normal_exit_signal;
        }
    }
    return not_an_exit_signal;
}

} // namespace <anonmyous>

namespace cppa { namespace detail {

yielding_message_queue_impl::queue_node::queue_node(const message& from)
    : next(0), msg(from)
{
}

yielding_message_queue_impl::yielding_message_queue_impl(scheduled_actor* p)
    : m_parent(p)
    , m_state(ready)
{
}

yielding_message_queue_impl::~yielding_message_queue_impl()
{
}

void yielding_message_queue_impl::enqueue(const message& msg)
{
    if (m_queue._push_back(new queue_node(msg)))
    {
        for (;;)
        {
            int state = m_state.load();
            switch (state)
            {
                case actor_state::blocked:
                {
                    if (m_state.compare_exchange_weak(state,actor_state::ready))
                    {
                        m_parent->enqueue_to_scheduler();
                        return;
                    }
                }
                case actor_state::about_to_block:
                {
                    if (m_state.compare_exchange_weak(state,actor_state::ready))
                    {
                        return;
                    }
                }
                default: return;
            }
        }
    }
}

void yielding_message_queue_impl::yield_until_not_empty()
{
    while (m_queue.empty())
    {
        m_state.store(actor_state::about_to_block);
        CPPA_MEMORY_BARRIER();
        //std::atomic_thread_fence(std::memory_order_seq_cst);
        // ensure that m_queue is empty
        if (!m_queue.empty())
        {
            // someone enqueued a message while we changed to 'about_to_block'
            m_state.store(actor_state::ready);
            return;
        }
        else
        {
            yield(yield_state::blocked);
            CPPA_MEMORY_BARRIER();
        }
    }
}

bool yielding_message_queue_impl::dequeue_impl(message& storage)
{
    yield_until_not_empty();
    std::unique_ptr<queue_node> node(m_queue.pop());
    if (!m_trap_exit)
    {
        if (yield_on_exit_msg(node->msg) == normal_exit_signal)
        {
            // exit_reason::normal is ignored by default,
            // dequeue next message
            return false;
        }
    }
    storage = std::move(node->msg);
    return true;
}

bool yielding_message_queue_impl::dequeue_impl(invoke_rules& rules,
                                               queue_node_buffer& buffer)
{
    yield_until_not_empty();
    std::unique_ptr<queue_node> node(m_queue.pop());
    if (!m_trap_exit)
    {
        if (yield_on_exit_msg(node->msg) == normal_exit_signal)
        {
            return false;
        }
    }
    std::unique_ptr<intermediate> imd(rules.get_intermediate(node->msg.content()));
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

} } // namespace cppa::detail
