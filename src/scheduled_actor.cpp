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

void scheduled_actor::run(void* _this)
{
    auto m_behavior = reinterpret_cast<scheduled_actor*>(_this)->m_behavior;
    if (m_behavior)
    {
        try { m_behavior->act(); }
        catch (...) { }
        try { m_behavior->on_exit(); }
        catch (...) { }
    }
    yield(yield_state::done);
}

void scheduled_actor::quit(std::uint32_t reason)
{
    super::cleanup(reason);
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
