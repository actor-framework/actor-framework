#ifndef THREAD_POOL_SCHEDULER_HPP
#define THREAD_POOL_SCHEDULER_HPP

#include <boost/thread.hpp>

#include "cppa/scheduler.hpp"
#include "cppa/detail/scheduled_actor.hpp"

namespace cppa { namespace detail {

class thread_pool_scheduler : public scheduler
{


    typedef util::single_reader_queue<scheduled_actor> job_queue;

    job_queue m_queue;
    scheduled_actor m_dummy;
    boost::thread m_supervisor;

 public:

    struct worker;

    thread_pool_scheduler();
    ~thread_pool_scheduler();

    void schedule(scheduled_actor* what);

    actor_ptr spawn(actor_behavior* behavior, scheduling_hint hint);

 private:

    static void worker_loop(worker*);
    static void supervisor_loop(job_queue*, scheduled_actor*);

};

} } // namespace cppa::detail

#endif // THREAD_POOL_SCHEDULER_HPP
