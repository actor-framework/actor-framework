#include <cstdint>
#include <cstddef>
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

namespace cppa { namespace detail {

namespace {

void enqueue_fun(cppa::detail::thread_pool_scheduler* where,
                 cppa::detail::scheduled_actor* what)
{
    where->schedule(what);
}

typedef boost::unique_lock<boost::mutex> guard_type;
typedef std::unique_ptr<thread_pool_scheduler::worker> worker_ptr;
typedef util::single_reader_queue<thread_pool_scheduler::worker> worker_queue;

} // namespace <anonmyous>

struct thread_pool_scheduler::worker
{

    worker* next;
    bool m_done;
    job_queue* m_job_queue;
    scheduled_actor* m_job;
    worker_queue* m_supervisor_queue;
    boost::thread m_thread;
    boost::mutex m_mtx;
    boost::condition_variable m_cv;

    worker(worker_queue* supervisor_queue, job_queue* jq)
        : next(nullptr), m_done(false), m_job_queue(jq), m_job(nullptr)
        , m_supervisor_queue(supervisor_queue)
        , m_thread(&thread_pool_scheduler::worker_loop, this)
    {
    }

    worker(const worker&) = delete;

    worker& operator=(const worker&) = delete;

    void operator()()
    {
        m_supervisor_queue->push_back(this);
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
            // run actor up to 300ms
            bool reschedule = false;
            boost::system_time tout = boost::get_system_time();
            tout += boost::posix_time::milliseconds(300);
            scheduled_actor::execute(m_job,
                                     fself,
                                     [&]() -> bool
                                     {
                                         if (tout >= boost::get_system_time())
                                         {
                                             reschedule = true;
                                             return false;
                                         }
                                         return true;
                                     },
                                     [m_job]()
                                     {
                                         if (!m_job->deref()) delete m_job;
                                         CPPA_MEMORY_BARRIER();
                                         actor_count::get().dec();
                                     });
            if (reschedule)
            {
                m_job_queue->push_back(m_job);
            }
            m_job = nullptr;
            CPPA_MEMORY_BARRIER();
            m_supervisor_queue->push_back(this);
        }
    }

};

void thread_pool_scheduler::worker_loop(thread_pool_scheduler::worker* w)
{
    (*w)();
}

void thread_pool_scheduler::supervisor_loop(job_queue* jqueue,
                                            scheduled_actor* dummy)
{
    worker_queue wqueue;
    std::vector<worker_ptr> workers;
    // init
    size_t num_workers = std::max(boost::thread::hardware_concurrency(),
                                  static_cast<unsigned>(1));
    for (size_t i = 0; i < num_workers; ++i)
    {
        workers.push_back(worker_ptr(new worker(&wqueue, jqueue)));
    }
    bool done = false;
    // loop
    do
    {
        // fetch job
        scheduled_actor* job = jqueue->pop();
        if (job == dummy)
        {
            done = true;
        }
        else
        {
            // fetch waiting worker (wait up to 500ms)
            worker* w = nullptr;
            while (!w)
            {
                w = wqueue.try_pop(500);
                // all workers are blocked since 500ms, start a new one
                if (!w)
                {
                    workers.push_back(worker_ptr(new worker(&wqueue,jqueue)));
                }
            }
            // lifetime scope of guard
            {
                guard_type guard(w->m_mtx);
                w->m_job = job;
                w->m_cv.notify_one();
            }
        }
    }
    while (!done);
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
    // "clear" worker_queue
    while (wqueue.try_pop() != nullptr) { }
}

thread_pool_scheduler::thread_pool_scheduler()
    : m_queue()
    , m_dummy()
    , m_supervisor(&thread_pool_scheduler::supervisor_loop, &m_queue, &m_dummy)
{
}

thread_pool_scheduler::~thread_pool_scheduler()
{
    m_queue.push_back(&m_dummy);
    m_supervisor.join();
}

void thread_pool_scheduler::schedule(scheduled_actor* what)
{
    m_queue.push_back(what);
}

actor_ptr thread_pool_scheduler::spawn(actor_behavior* behavior,
                                       scheduling_hint hint)
{
    if (hint == detached)
    {
        return mock_scheduler::spawn(behavior);
    }
    else
    {
        actor_count::get().inc();
        CPPA_MEMORY_BARRIER();
        intrusive_ptr<scheduled_actor> ctx(new scheduled_actor(behavior,
                                                               enqueue_fun,
                                                               this));
        ctx->ref();
        m_queue.push_back(ctx.get());
        return ctx;
    }
}

} } // namespace cppa::detail
