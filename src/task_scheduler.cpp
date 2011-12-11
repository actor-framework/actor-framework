#include <iostream>

#include "cppa/config.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/util/fiber.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/detail/invokable.hpp"
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/task_scheduler.hpp"
#include "cppa/detail/yielding_actor.hpp"
#include "cppa/detail/yield_interface.hpp"
#include "cppa/detail/converted_thread_context.hpp"

using std::cout;
using std::cerr;
using std::endl;

namespace {

void enqueue_fun(cppa::detail::task_scheduler* where,
                 cppa::detail::scheduled_actor* what)
{
    where->schedule(what);
}

} // namespace <anonmyous>

namespace cppa { namespace detail {

void task_scheduler::worker_loop(job_queue* jq, scheduled_actor* dummy)
{
    cppa::util::fiber fself;
    scheduled_actor* job = nullptr;
    struct handler : scheduled_actor::resume_callback
    {
        scheduled_actor** job_ptr;
        handler(scheduled_actor** jptr) : job_ptr(jptr) { }
        bool still_ready()
        {
            return true;
        }
        void exec_done()
        {
            auto job = *job_ptr;
            if (!job->deref()) delete job;
            CPPA_MEMORY_BARRIER();
            dec_actor_count();
        }
    };
    handler h(&job);
    for (;;)
    {
        job = jq->pop();
        if (job == dummy)
        {
            return;
        }
        job->resume(&fself, &h);
    }
}

void task_scheduler::start()
{
    super::start();
    m_worker = thread(worker_loop, &m_queue, &m_dummy);
}

void task_scheduler::stop()
{
    m_queue.push_back(&m_dummy);
    m_worker.join();
    super::stop();
}

void task_scheduler::schedule(scheduled_actor* what)
{
    if (what)
    {
        if (this_thread::get_id() == m_worker.get_id())
        {
            m_queue._push_back(what);
        }
        else
        {
            m_queue.push_back(what);
        }
    }
}

actor_ptr task_scheduler::spawn_impl(scheduled_actor* what)
{
    inc_actor_count();
    CPPA_MEMORY_BARRIER();
    intrusive_ptr<scheduled_actor> ctx(what);
    // add an implicit reference to ctx
    ctx->ref();
    m_queue.push_back(ctx.get());
    return std::move(ctx);
}


actor_ptr task_scheduler::spawn(abstract_event_based_actor* what)
{
    return spawn_impl(what->attach_to_scheduler(enqueue_fun, this));
}

actor_ptr task_scheduler::spawn(actor_behavior* behavior, scheduling_hint)
{
    return spawn_impl(new yielding_actor(behavior, enqueue_fun, this));
}

} } // namespace cppa::detail
