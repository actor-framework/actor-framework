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

struct exit_observer : cppa::attachable
{
    ~exit_observer()
    {
        cppa::detail::dec_actor_count();
    }
};

} // namespace <anonymous>

namespace cppa { namespace detail {

actor_ptr mock_scheduler::spawn(actor_behavior* ab, scheduling_hint)
{
    inc_actor_count();
    intrusive_ptr<context> ctx(new detail::converted_thread_context);
    boost::thread(run_actor, ctx, ab).detach();
    return ctx;
}

void mock_scheduler::register_converted_context(context* ctx)
{
    if (ctx)
    {
        inc_actor_count();
        ctx->attach(new exit_observer);
    }
}

attachable* mock_scheduler::register_hidden_context()
{
    inc_actor_count();
    return new exit_observer;
}

void mock_scheduler::await_others_done()
{
    actor_count_wait_until((unchecked_self() == nullptr) ? 0 : 1);
}

} } // namespace detail
