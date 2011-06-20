#include <limits>
#include <atomic>
#include <stdexcept>
#include <boost/thread.hpp>

#include "cppa/detail/actor_count.hpp"

namespace {

boost::mutex s_ra_mtx;
boost::condition_variable s_ra_cv;
std::atomic<size_t> s_running_actors(0);

typedef boost::unique_lock<boost::mutex> guard_type;

} // namespace <anonymous>

namespace cppa { namespace detail {

void inc_actor_count()
{
    ++s_running_actors;
}

void dec_actor_count()
{
    size_t new_val = --s_running_actors;
    if (new_val == std::numeric_limits<size_t>::max())
    {
        throw std::underflow_error("dec_actor_count()");
    }
    else if (new_val <= 1)
    {
        guard_type guard(s_ra_mtx);
        s_ra_cv.notify_all();
    }
}

void actor_count_wait_until(size_t expected)
{
    guard_type lock(s_ra_mtx);
    while (s_running_actors.load() != expected)
    {
        s_ra_cv.wait(lock);
    }
}

exit_observer::~exit_observer()
{
    cppa::detail::dec_actor_count();
}

} } // namespace cppa::detail
