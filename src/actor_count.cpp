#include <atomic>
#include <limits>
#include <stdexcept>

#include "cppa/detail/thread.hpp"
#include "cppa/detail/actor_count.hpp"

namespace {

typedef cppa::detail::unique_lock<cppa::detail::mutex> guard_type;

class actor_count
{

    cppa::detail::mutex m_ra_mtx;
    cppa::detail::condition_variable m_ra_cv;
    std::atomic<size_t> m_running_actors;

 public:

    actor_count() : m_running_actors(0) { }

    void inc()
    {
        ++m_running_actors;
    }

    void dec()
    {
        size_t new_val = --m_running_actors;
        if (new_val == std::numeric_limits<size_t>::max())
        {
            throw std::underflow_error("actor_count::dec()");
        }
        else if (new_val <= 1)
        {
            guard_type guard(m_ra_mtx);
            m_ra_cv.notify_all();
        }
    }

    void wait_until(size_t expected)
    {
        guard_type lock(m_ra_mtx);
        while (m_running_actors.load() != expected)
        {
            m_ra_cv.wait(lock);
        }
    }

};

// TODO: free
actor_count* s_actor_count = new actor_count;

} // namespace <anonymous>

namespace cppa { namespace detail {

void inc_actor_count()
{
    s_actor_count->inc();
}

void dec_actor_count()
{
    s_actor_count->dec();
}

void actor_count_wait_until(size_t expected)
{
    s_actor_count->wait_until(expected);
}

} } // namespace cppa::detail
