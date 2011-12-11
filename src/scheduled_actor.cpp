#include "cppa/exception.hpp"
#include "cppa/detail/task_scheduler.hpp"
#include "cppa/detail/scheduled_actor.hpp"
#include "cppa/detail/yield_interface.hpp"

namespace { void dummy_enqueue(void*, cppa::detail::scheduled_actor*) { } }

namespace cppa { namespace detail {

scheduled_actor::scheduled_actor()
    : next(nullptr)
    , m_state(scheduled_actor::done)
    , m_enqueue_to_scheduler(dummy_enqueue, static_cast<void*>(nullptr), this)
{
}

scheduled_actor::resume_callback::~resume_callback()
{
}

void scheduled_actor::quit(std::uint32_t reason)
{
    cleanup(reason);
    throw actor_exited(reason);
    //yield(yield_state::done);
}

void scheduled_actor::enqueue_node(queue_node* node)
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

void scheduled_actor::enqueue(actor* sender, any_tuple&& msg)
{
    enqueue_node(new queue_node(sender, std::move(msg)));
}

void scheduled_actor::enqueue(actor* sender, const any_tuple& msg)
{
    enqueue_node(new queue_node(sender, msg));
}

int scheduled_actor::compare_exchange_state(int expected, int new_value)
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
