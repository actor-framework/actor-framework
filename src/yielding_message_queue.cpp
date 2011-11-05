#include "cppa/config.hpp"

#include <mutex>
#include <atomic>
#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/scheduler.hpp"
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

} // namespace <anonmyous>

namespace cppa { namespace detail {

yielding_message_queue_impl::queue_node::queue_node(actor* ptr, any_tuple&& tup)
    : next(0), sender(ptr), msg(std::move(tup))
{
}

yielding_message_queue_impl::queue_node::queue_node(actor* ptr,
                                                    const any_tuple& tup)
    : next(0), sender(ptr), msg(tup)
{
}

yielding_message_queue_impl::yielding_message_queue_impl(scheduled_actor* p)
    : m_has_pending_timeout_request(false), m_active_timeout_id(0)
    , m_parent(p), m_state(ready)
    , m_atom_value_uti(uniform_typeid<atom_value>())
    , m_ui32_uti(uniform_typeid<std::uint32_t>())
    , m_actor_ptr_uti(uniform_typeid<actor_ptr>())
{
}

yielding_message_queue_impl::~yielding_message_queue_impl()
{
}

enum filter_result
{
    normal_exit_signal,
    expired_timeout_message,
    timeout_message,
    ordinary_message
};

yielding_message_queue_impl::filter_result
yielding_message_queue_impl::filter_msg(const any_tuple& msg)
{
    if (msg.size() == 2 && m_atom_value_uti == &(msg.utype_info_at(0)))
    {
        switch ((std::uint64_t) *reinterpret_cast<const atom_value*>(msg.at(0)))
        {
            case atom(":Exit"):
            {
                if (m_trap_exit == false)
                {
                    if (m_ui32_uti == &(msg.utype_info_at(1)))
                    {
                        auto why = *reinterpret_cast<const std::uint32_t*>(msg.at(1));
                        if (why != exit_reason::normal)
                        {
                            self()->quit(why);
                        }
                        return normal_exit_signal;
                    }
                }
                break;
            }
            case atom(":Timeout"):
            {
                if (m_ui32_uti == &(msg.utype_info_at(1)))
                {
                    auto id = *reinterpret_cast<const std::uint32_t*>(msg.at(1));
                    return (id == m_active_timeout_id) ? timeout_message
                                                       : expired_timeout_message;
                }
                break;
            }
            default: break; /* nop */
        }
    }
    return ordinary_message;
}

void yielding_message_queue_impl::enqueue_node(queue_node* node)
{
    if (m_queue._push_back(node))
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

void yielding_message_queue_impl::enqueue(actor* sender, any_tuple&& msg)
{
    enqueue_node(new queue_node(sender, std::move(msg)));
}

void yielding_message_queue_impl::enqueue(actor* sender, const any_tuple& msg)
{
    enqueue_node(new queue_node(sender, msg));
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
            //CPPA_MEMORY_BARRIER();
        }
    }
}

bool yielding_message_queue_impl::dequeue_impl(any_tuple& storage)
{
    yield_until_not_empty();
    std::unique_ptr<queue_node> node(m_queue.pop());
    if (filter_msg(node->msg) != ordinary_message)
    {
        // dequeue next message
        return false;
    }
    storage = std::move(node->msg);
    return true;
}

yielding_message_queue_impl::dq_result
yielding_message_queue_impl::dq(std::unique_ptr<queue_node>& node,
                                invoke_rules_base& rules,
                                queue_node_buffer& buffer)
{
    switch (filter_msg(node->msg))
    {
        case normal_exit_signal:
        case expired_timeout_message:
            // discard message
            return indeterminate;

        case timeout_message:
            return timeout_occured;

        default: break;
    }
    auto imd = rules.get_intermediate(node->msg);
    if (imd)
    {
        m_last_dequeued = node->msg;
        // restore mailbox before invoking imd
        if (!buffer.empty()) m_queue.push_front(std::move(buffer));
        imd->invoke();
        return done;
    }
    else
    {
        buffer.push_back(node.release());
        return indeterminate;
    }
}

bool yielding_message_queue_impl::dequeue_impl(timed_invoke_rules& rules,
                                               queue_node_buffer& buffer)
{
    if (m_queue.empty() && !m_has_pending_timeout_request)
    {
        future_send(self(), rules.timeout(),
                    atom(":Timeout"), ++m_active_timeout_id);
        m_has_pending_timeout_request = true;
    }
    yield_until_not_empty();
    std::unique_ptr<queue_node> node(m_queue.pop());
    switch (dq(node, rules, buffer))
    {
        case done:
            if (m_has_pending_timeout_request)
            {
                // expire pending request
                ++m_active_timeout_id;
                m_has_pending_timeout_request = false;
            }
            return true;

        case timeout_occured:
            rules.handle_timeout();
            m_has_pending_timeout_request = false;
            return true;

        case indeterminate:
            return false;
    }
    return false;
}

bool yielding_message_queue_impl::dequeue_impl(invoke_rules& rules,
                                               queue_node_buffer& buffer)
{
    yield_until_not_empty();
    std::unique_ptr<queue_node> node(m_queue.pop());
    return dq(node, rules, buffer) != indeterminate;
}

} } // namespace cppa::detail
