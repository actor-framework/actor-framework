#ifndef CPPA_DETAIL_CPP0X_THREAD_WRAPPER_HPP
#define CPPA_DETAIL_CPP0X_THREAD_WRAPPER_HPP

// This header imports C++0x compatible parts of the boost threading library
// to the std namespace. It's a workaround to the missing stl threading
// library on GCC 4.6 under Mac OS X

#include <boost/thread.hpp>

namespace std {

using boost::mutex;
using boost::recursive_mutex;
using boost::timed_mutex;
using boost::recursive_timed_mutex;
using boost::adopt_lock;
using boost::defer_lock;
using boost::try_to_lock;

using boost::lock_guard;
using boost::unique_lock;

} // namespace std

#endif // CPPA_DETAIL_CPP0X_THREAD_WRAPPER_HPP
