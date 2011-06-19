#include <atomic>

#define _GLIBCXX_HAS_GTHREADS
#include <mutex>

#include "cppa/context.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/detail/mock_scheduler.hpp"

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
        scheduler* new_instance = new detail::mock_scheduler;
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
