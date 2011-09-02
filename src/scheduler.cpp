#include <atomic>

#ifndef _GLIBCXX_HAS_GTHREADS
#define _GLIBCXX_HAS_GTHREADS
#endif
#include <mutex>

#include "cppa/context.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"
#include "cppa/detail/converted_thread_context.hpp"

namespace {

std::atomic<cppa::scheduler*> m_instance;

/*
struct static_cleanup_helper
{
    ~static_cleanup_helper()
    {
        auto i = m_instance.load();
        while (i)
        {
            if (m_instance.compare_exchange_weak(i, nullptr))
            {
                delete i;
                i = nullptr;
            }
        }
    }
}
s_cleanup_helper;
*/

} // namespace <anonymous>

namespace cppa {

class scheduler_helper
{

    cppa::intrusive_ptr<cppa::context> m_worker;

    static void worker_loop(cppa::intrusive_ptr<cppa::context> m_self);

 public:

    scheduler_helper() : m_worker(new detail::converted_thread_context)
    {
        // do NOT increase actor count; worker is "invisible"
        boost::thread(&scheduler_helper::worker_loop, m_worker).detach();
    }

    ~scheduler_helper()
    {
        m_worker->enqueue(message(m_worker, m_worker, atom(":_DIE")));
    }

};

void scheduler_helper::worker_loop(cppa::intrusive_ptr<cppa::context> m_self)
{
    set_self(m_self.get());
}

scheduler::scheduler() : m_helper(new scheduler_helper)
{
}

scheduler::~scheduler()
{
    delete m_helper;
}

void scheduler::await_others_done()
{
    detail::actor_count::get().wait_until((unchecked_self() == nullptr) ? 0 : 1);
}

void scheduler::register_converted_context(context* what)
{
    if (what)
    {
        detail::actor_count::get().inc();
        what->attach(new detail::exit_observer);
    }
}

attachable* scheduler::register_hidden_context()
{
    detail::actor_count::get().inc();
    return new detail::exit_observer;
}

void scheduler::exit_context(context* ctx, std::uint32_t reason)
{
    ctx->quit(reason);
}

void set_scheduler(scheduler* sched)
{
    scheduler* s = nullptr;
    if (m_instance.compare_exchange_weak(s, sched) == false)
    {
        throw std::runtime_error("scheduler already set");
    }
}

scheduler* get_scheduler()
{
    scheduler* result = m_instance.load();
    while (result == nullptr)
    {
        scheduler* new_instance = new detail::thread_pool_scheduler;
        if (m_instance.compare_exchange_weak(result, new_instance))
        {
            result = new_instance;
        }
        else
        {
            delete new_instance;
        }
    }
    return result;
}

} // namespace cppa
