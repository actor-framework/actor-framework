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
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/converted_thread_context.hpp"

using std::cout;
using std::endl;

namespace {

boost::mutex s_ra_mtx;
boost::condition_variable s_ra_cv;
std::atomic<int> s_running_actors(0);

typedef boost::unique_lock<boost::mutex> guard_type;

void inc_actor_count()
{
     ++s_running_actors;
}

void dec_actor_count()
{
    if (--s_running_actors <= 1)
    {
        guard_type guard(s_ra_mtx);
        s_ra_cv.notify_all();
    }
}

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
    dec_actor_count();
}

struct exit_observer : cppa::attachable
{
    ~exit_observer()
    {
        dec_actor_count();
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
    auto expected = (unchecked_self() == nullptr) ? 0 : 1;
    guard_type lock(s_ra_mtx);
    while (s_running_actors.load() != expected)
    {
        s_ra_cv.wait(lock);
    }
}

} } // namespace detail
