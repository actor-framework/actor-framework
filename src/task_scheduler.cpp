#include <iostream>
#include <boost/thread.hpp>

#include "cppa/config.hpp"
#include "cppa/context.hpp"
#include "cppa/util/fiber.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/task_scheduler.hpp"
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
    for (;;)
    {
        scheduled_actor* job = jq->pop();
        if (job == dummy)
        {
            return;
        }
        scheduled_actor::execute(job, fself, [&]()
        {
            if (!job->deref()) delete job;
            CPPA_MEMORY_BARRIER();
            actor_count::get().dec();
        });
    }
}

task_scheduler::task_scheduler()
    : m_queue()
    , m_dummy()
{
    m_worker = boost::thread(worker_loop, &m_queue, &m_dummy);
}

task_scheduler::~task_scheduler()
{
    m_queue.push_back(&m_dummy);
    m_worker.join();
}

void task_scheduler::schedule(scheduled_actor* what)
{
    if (what)
    {
        if (boost::this_thread::get_id() == m_worker.get_id())
        {
            m_queue._push_back(what);
        }
        else
        {
            m_queue.push_back(what);
        }
    }
}

actor_ptr task_scheduler::spawn(actor_behavior* behavior, scheduling_hint)
{
    actor_count::get().inc();
    intrusive_ptr<scheduled_actor> ctx(new scheduled_actor(behavior,
                                                           enqueue_fun,
                                                           this));
    // add an implicit reference to ctx
    ctx->ref();
    m_queue.push_back(ctx.get());
    return ctx;
}

} } // namespace cppa::detail
