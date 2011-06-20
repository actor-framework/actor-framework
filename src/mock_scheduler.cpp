#include "cppa/config.hpp"

#include <atomic>
#include <iostream>

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include "cppa/message.hpp"
#include "cppa/context.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/attachable.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/converted_thread_context.hpp"

using std::cout;
using std::endl;

namespace {

void run_actor(cppa::intrusive_ptr<cppa::context> m_self,
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

actor_ptr mock_scheduler::spawn(actor_behavior* ab, scheduling_hint)
{
    inc_actor_count();
    intrusive_ptr<context> ctx(new detail::converted_thread_context);
    boost::thread(run_actor, ctx, ab).detach();
    return ctx;
}

} } // namespace detail
