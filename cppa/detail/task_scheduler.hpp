#ifndef TASK_SCHEDULER_HPP
#define TASK_SCHEDULER_HPP

#include <boost/thread.hpp>

#include "cppa/scheduler.hpp"
#include "cppa/detail/scheduled_actor.hpp"
#include "cppa/util/single_reader_queue.hpp"

namespace cppa { namespace detail {

class task_scheduler : public scheduler
{

    typedef util::single_reader_queue<scheduled_actor> job_queue;

    job_queue m_queue;
    scheduled_actor m_dummy;
    boost::thread m_worker;

    static void worker_loop(job_queue*, scheduled_actor* dummy);

 public:

    task_scheduler();
    ~task_scheduler();

    void schedule(scheduled_actor* what);

    void await_others_done();
    void register_converted_context(context*);
    //void unregister_converted_context(context*);
    actor_ptr spawn(actor_behavior*, scheduling_hint);
    attachable* register_hidden_context();

};

} } // namespace cppa::detail

#endif // TASK_SCHEDULER_HPP
