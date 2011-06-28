#include <cstdint>
#include <cstddef>
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

namespace {

void enqueue_fun(cppa::detail::thread_pool_scheduler* where,
                 cppa::detail::scheduled_actor* what)
{
    where->schedule(what);
}

typedef boost::unique_lock<boost::mutex> guard_type;

} // namespace <anonmyous>

namespace cppa { namespace detail {

struct thread_pool_scheduler::worker
{

    worker* next;
    bool m_done;
    scheduled_actor* m_job;
    util::single_reader_queue<worker>* m_supervisor_queue;
    boost::thread m_thread;
    boost::mutex m_mtx;
    boost::condition_variable m_cv;

    worker(util::single_reader_queue<worker>* supervisor_queue)
        : next(nullptr), m_done(false), m_job(nullptr)
        , m_supervisor_queue(supervisor_queue)
        , m_thread(&thread_pool_scheduler::worker_loop, this)
    {
    }

    worker(const worker&) = delete;

    worker& operator=(const worker&) = delete;

    void operator()()
    {
        // loop
        util::fiber fself;
        for (;;)
        {
            // lifetime scope of guard (wait for new job)
            {
                guard_type guard(m_mtx);
                while (m_job == nullptr && !m_done)
                {
                    m_cv.wait(guard);
                }
                if (m_done) return;
            }
            scheduled_actor::execute(m_job, fself, []()
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
    util::single_reader_queue<worker> worker_queue;
    std::vector<std::unique_ptr<worker>> workers;
    // init
    size_t num_workers = std::max(boost::thread::hardware_concurrency(),
                                  static_cast<unsigned>(1));
    for (size_t i = 0; i < num_workers; ++i)
    {
        workers.push_back(std::unique_ptr<worker>(new worker(&worker_queue)));
    }
    // loop
    for (;;)
    {
        // fetch job
        scheduled_actor* job = jq->pop();
        // fetch waiting worker (wait up to 500ms)
        worker* w = nullptr;
        while (!w)
        {
            w = worker_queue.try_pop(500);
            if (!w)
            {
                workers.push_back(std::unique_ptr<worker>(new worker(&worker_queue)));
            }
        }
        // lifetime scope of guard
        {
            guard_type guard(w->m_mtx);
            w->m_job = job;
            w->m_cv.notify_one();
        }
    }
    // quit
    for (auto& w : workers)
    {
        guard_type guard(w->m_mtx);
        w->m_done = true;
        w->m_cv.notify_one();
    }
    // wait for workers
    for (auto& w : workers)
    {
        w->m_thread.join();
    }
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
