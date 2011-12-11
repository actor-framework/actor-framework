#ifndef TASK_SCHEDULER_HPP
#define TASK_SCHEDULER_HPP

#include "cppa/scheduler.hpp"
#include "cppa/detail/thread.hpp"
#include "cppa/detail/scheduled_actor.hpp"
#include "cppa/util/single_reader_queue.hpp"

namespace cppa { namespace detail {

class task_scheduler : public scheduler
{

    typedef scheduler super;

    typedef util::single_reader_queue<scheduled_actor> job_queue;

    job_queue m_queue;
    scheduled_actor_dummy m_dummy;
    thread m_worker;

    static void worker_loop(job_queue*, scheduled_actor* dummy);

 public:

    virtual void start();

    virtual void stop();

    void schedule(scheduled_actor* what);

    actor_ptr spawn(abstract_event_based_actor* what);

    actor_ptr spawn(actor_behavior*, scheduling_hint);

 private:

    actor_ptr spawn_impl(scheduled_actor* what);

};

} } // namespace cppa::detail

#endif // TASK_SCHEDULER_HPP
