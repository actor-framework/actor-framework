#include <limits>
#include <stdexcept>

#include "cppa/detail/actor_count.hpp"

namespace {

typedef boost::unique_lock<boost::mutex> guard_type;

} // namespace <anonymous>

namespace cppa { namespace detail {

// TODO: free
actor_count* actor_count::s_instance = new actor_count();

actor_count& actor_count::get()
{
    return *s_instance;
}

actor_count::actor_count() : m_running_actors(0)
{
}

void actor_count::inc()
{
    ++m_running_actors;
}

void actor_count::dec()
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

void actor_count::wait_until(size_t expected)
{
    guard_type lock(m_ra_mtx);
    while (m_running_actors.load() != expected)
    {
        m_ra_cv.wait(lock);
    }
}

exit_observer::~exit_observer()
{
    actor_count::get().dec();
}

} } // namespace cppa::detail
