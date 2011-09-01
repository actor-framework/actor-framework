#ifndef THREAD_HPP
#define THREAD_HPP

#ifdef __APPLE__

#include <boost/thread.hpp>

namespace cppa { namespace detail {

using boost::mutex;
using boost::thread;
using boost::condition_variable;

} } // namespace cppa::detail

#else

#include <mutex>
#include <thread>
#include <condition_variable>

namespace cppa { namespace detail {

using std::mutex;
using std::thread;
using std::condition_variable;
using std::unique_lock;

namespace this_thread { using namespace std::this_thread; }

// returns false if a timeout occured
template<class Lock, class Condition, typename TimePoint>
inline bool wait_until(Lock& lock, Condition& cond, const TimePoint& timeout)
{
    return cond.wait_until(lock, timeout) != std::cv_status::timeout;
}

inline auto now() -> decltype(std::chrono::high_resolution_clock::now())
{
    return std::chrono::high_resolution_clock::now();
}

} } // namespace cppa::detail

#endif // __APPLE__

#endif // THREAD_HPP
