#include "cppa/cppa.hpp"
#include "cppa/exception.hpp"
#include "cppa/detail/task_scheduler.hpp"
#include "cppa/detail/abstract_scheduled_actor.hpp"
#include "cppa/detail/yield_interface.hpp"

namespace cppa { namespace detail {

namespace { void dummy_enqueue(void*, abstract_scheduled_actor*) { } }

abstract_scheduled_actor::abstract_scheduled_actor()
    : next(nullptr)
    , m_state(abstract_scheduled_actor::done)
    , m_enqueue_to_scheduler(dummy_enqueue, static_cast<void*>(nullptr), this)
    , m_has_pending_timeout_request(false)
    , m_active_timeout_id(0)
{
}

abstract_scheduled_actor::resume_callback::~resume_callback()
{
}

void abstract_scheduled_actor::quit(std::uint32_t reason)
{
    cleanup(reason);
    throw actor_exited(reason);
}

void abstract_scheduled_actor::enqueue_node(queue_node* node)
{
    if (m_mailbox._push_back(node))
    {
        for (;;)
        {
            int state = m_state.load();
            switch (state)
            {
                case blocked:
                {
                    if (m_state.compare_exchange_weak(state, ready))
                    {
                        m_enqueue_to_scheduler();
                        return;
                    }
                    break;
                }
                case about_to_block:
                {
                    if (m_state.compare_exchange_weak(state, ready))
                    {
                        return;
                    }
                    break;
                }
                default: return;
            }
        }
    }
}

void abstract_scheduled_actor::enqueue(actor* sender, any_tuple&& msg)
{
    enqueue_node(new queue_node(sender, std::move(msg)));
}

void abstract_scheduled_actor::enqueue(actor* sender, const any_tuple& msg)
{
    enqueue_node(new queue_node(sender, msg));
}

int abstract_scheduled_actor::compare_exchange_state(int expected,
                                                     int new_value)
{
    int e = expected;
    do
    {
        if (m_state.compare_exchange_weak(e, new_value))
        {
            return new_value;
        }
    }
    while (e == expected);
    return e;
}

void abstract_scheduled_actor::request_timeout(const util::duration& d)
{
    future_send(this, d, atom(":Timeout"), ++m_active_timeout_id);
    m_has_pending_timeout_request = true;
}

auto abstract_scheduled_actor::filter_msg(const any_tuple& msg) -> filter_result
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

auto abstract_scheduled_actor::dq(std::unique_ptr<queue_node>& node,
                                  invoke_rules_base& rules,
                                  queue_node_buffer& buffer) -> dq_result
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
            if (!buffer.empty())
            {
                m_mailbox.push_front(std::move(buffer));
                buffer.clear();
            }
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
        if (!buffer.empty())
        {
            m_mailbox.push_front(std::move(buffer));
            buffer.clear();
        }
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

// dummy

void scheduled_actor_dummy::resume(util::fiber*, resume_callback*)
{
}

void scheduled_actor_dummy::quit(std::uint32_t)
{
}

void scheduled_actor_dummy::dequeue(invoke_rules&)
{
}

void scheduled_actor_dummy::dequeue(timed_invoke_rules&)
{
}

void scheduled_actor_dummy::link_to(intrusive_ptr<actor>&)
{
}

void scheduled_actor_dummy::unlink_from(intrusive_ptr<actor>&)
{
}

bool scheduled_actor_dummy::establish_backlink(intrusive_ptr<actor>&)
{
    return false;
}

bool scheduled_actor_dummy::remove_backlink(intrusive_ptr<actor>&)
{
    return false;
}

void scheduled_actor_dummy::detach(const attachable::token&)
{
}

bool scheduled_actor_dummy::attach(attachable*)
{
    return false;
}

} } // namespace cppa::detail
