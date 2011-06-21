#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

namespace {

void enqueue_fun(cppa::detail::thread_pool_scheduler* where,
                 cppa::detail::scheduled_actor* what)
{
    where->schedule(what);
}

} // namespace <anonmyous>

namespace cppa { namespace detail {

struct thread_pool_scheduler::worker
{

    scheduled_actor* m_job;
    boost::mutex m_mtx;
    boost::condition_variable m_cv;

    worker() : m_job(nullptr)
    {
    }

    void operator()()
    {
        util::fiber fself;
        for (;;)
        {
            // lifetime scope of guard
            {
                boost::unique_lock<boost::mutex> guard(m_mtx);
                while (m_job == nullptr)
                {
                    m_cv.wait(guard);
                }
            }
            scheduled_actor::execute(m_job, fself, [](yield_state ystate)
            {
            });
        }
    }

};

void thread_pool_scheduler::worker_loop(thread_pool_scheduler::worker* w)
{
    (*w)();
}

void thread_pool_scheduler::supervisor_loop(job_queue* jq,
                                            scheduled_actor* dummy)
{
}

thread_pool_scheduler::thread_pool_scheduler()
    : m_queue()
    , m_dummy()
    , m_supervisor(&thread_pool_scheduler::supervisor_loop, &m_queue, &m_dummy)
{
}

thread_pool_scheduler::~thread_pool_scheduler()
{
}

void thread_pool_scheduler::schedule(scheduled_actor* what)
{
    m_queue.push_back(what);
}

actor_ptr thread_pool_scheduler::spawn(actor_behavior* behavior,
                                       scheduling_hint hint)
{
    inc_actor_count();
    intrusive_ptr<scheduled_actor> ctx(new scheduled_actor(behavior,
                                                           enqueue_fun,
                                                           this));
    ctx->ref();
    m_queue.push_back(ctx.get());
    return ctx;
}

} } // namespace cppa::detail
