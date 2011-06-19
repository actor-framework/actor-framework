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

struct exit_observer : cppa::attachable
{
    ~exit_observer()
    {
        cppa::detail::dec_actor_count();
    }
};

} // namespace <anonymous>

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
        cppa::set_self(job);
        call(&(job->m_fiber), &fself);
        switch (yielded_state())
        {
            case yield_state::ready:
            {
                // call again (suppress mutex / condition variable)
                jq->_push_back(job);
                break;
            }
            case yield_state::blocked:
            {
                // wait until someone re-schedules that actor
                int s = job->state().load();
                while (s == about_to_block)
                {
                    if (job->state().compare_exchange_weak(s, blocked))
                    {
                        s = blocked;
                    }
                }
                switch (s)
                {
                    case ready:
                    {
                        // someone just interleaved "blocking process"
                        jq->push_back(job);
                        break;
                    }
                    case done:
                    {
                        // must not happen
                        cerr << "state == done after about_to_block" << endl;
                        exit(7);
                    }
                    case about_to_block:
                    {
                        cerr << "state == about_to_block after "
                                "setting to blocked" << endl;
                        exit(7);
                    }
                    case blocked:
                    {
                        break;
                    }
                    default:
                    {
                        cerr << "illegal state" << endl;
                        exit(7);
                    }
                }
                break;
            }
            case yield_state::done:
            case yield_state::killed:
            {
                if (!job->deref()) delete job;
                CPPA_MEMORY_BARRIER();
                dec_actor_count();
                break;
            }
            default:
            {
                exit(7);
            }
        }
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
    if (what) m_queue.push_back(what);
}

void task_scheduler::await_others_done()
{
    actor_count_wait_until((unchecked_self() == nullptr) ? 0 : 1);
}

void task_scheduler::register_converted_context(context* ctx)
{
    if (ctx)
    {
        inc_actor_count();
        ctx->attach(new exit_observer);
    }
}

actor_ptr task_scheduler::spawn(actor_behavior* behavior, scheduling_hint)
{
    inc_actor_count();
    intrusive_ptr<scheduled_actor> ctx(new scheduled_actor(behavior, this));
    // add an implicit reference to ctx
    ctx->ref();
    m_queue.push_back(ctx.get());
    return ctx;
}

attachable* task_scheduler::register_hidden_context()
{
    inc_actor_count();
    return new exit_observer;
}

} } // namespace cppa::detail
