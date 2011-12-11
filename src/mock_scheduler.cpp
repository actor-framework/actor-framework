#include "cppa/config.hpp"

#include <atomic>
#include <iostream>


#include "cppa/any_tuple.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/attachable.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/actor_behavior.hpp"

#include "cppa/detail/thread.hpp"
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/converted_thread_context.hpp"

using std::cout;
using std::endl;

namespace {

void run_actor(cppa::intrusive_ptr<cppa::local_actor> m_self,
               cppa::actor_behavior* behavior)
{
    cppa::set_self(m_self.get());
    if (behavior)
    {
        try { behavior->act(); }
        catch (...) { }
        try { behavior->on_exit(); }
        catch (...) { }
        delete behavior;
    }
    cppa::detail::dec_actor_count();
}

} // namespace <anonymous>

namespace cppa { namespace detail {

actor_ptr mock_scheduler::spawn(actor_behavior* behavior)
{
    inc_actor_count();
    CPPA_MEMORY_BARRIER();
    intrusive_ptr<local_actor> ctx(new detail::converted_thread_context);
    thread(run_actor, ctx, behavior).detach();
    return ctx;
}

actor_ptr mock_scheduler::spawn(event_based_actor* what)
{
    // TODO: don't delete what :)
    delete what;
    return nullptr;
}

actor_ptr mock_scheduler::spawn(actor_behavior* behavior, scheduling_hint)
{
    return spawn(behavior);
}

} } // namespace detail
