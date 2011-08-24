#include <iostream>

#include "cppa/exception.hpp"
#include "cppa/detail/task_scheduler.hpp"
#include "cppa/detail/scheduled_actor.hpp"
#include "cppa/detail/yield_interface.hpp"

using std::cout;
using std::endl;

namespace { void dummy_enqueue(void*, cppa::detail::scheduled_actor*) { } }

namespace cppa { namespace detail {

scheduled_actor::scheduled_actor()
    : next(nullptr)
    , m_fiber()
    , m_behavior(nullptr)
    , m_mailbox(this)
    , m_enqueue_to_scheduler(dummy_enqueue, static_cast<void*>(nullptr), this)
{
}

scheduled_actor::~scheduled_actor()
{
    //cout << __PRETTY_FUNCTION__ << endl;
    delete m_behavior;
}

void scheduled_actor::run(void* ptr_arg)
{
    auto this_ptr = reinterpret_cast<scheduled_actor*>(ptr_arg);
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

void scheduled_actor::quit(std::uint32_t reason)
{
    cleanup(reason);
    throw actor_exited(reason);
    //yield(yield_state::done);
}

message_queue& scheduled_actor::mailbox()
{
    return m_mailbox;
}

const message_queue& scheduled_actor::mailbox() const
{
    return m_mailbox;
}

} } // namespace cppa::detail
