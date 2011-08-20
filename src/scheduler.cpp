#include <atomic>

#ifndef _GLIBCXX_HAS_GTHREADS
#define _GLIBCXX_HAS_GTHREADS
#endif
#include <mutex>

#include "cppa/context.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/detail/actor_count.hpp"
//#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

namespace {

//std::mutex m_instance_mtx;
std::atomic<cppa::scheduler*> m_instance;

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

} // namespace <anonymous>

namespace cppa {

scheduler::~scheduler()
{
}

void scheduler::await_others_done()
{
    detail::actor_count_wait_until((unchecked_self() == nullptr) ? 0 : 1);
}

void scheduler::register_converted_context(context* what)
{
    if (what)
    {
        detail::inc_actor_count();
        what->attach(new detail::exit_observer);
    }
}

attachable* scheduler::register_hidden_context()
{
    detail::inc_actor_count();
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
        throw std::runtime_error("cannot set scheduler");
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
