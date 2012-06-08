/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef THREAD_HPP
#define THREAD_HPP

#if defined(__APPLE__) && (__GNUC__ == 4) && (__GNUC_MINOR__ < 7) && !defined(__clang__)

#define CPPA_USE_BOOST_THREADS

#include <boost/thread.hpp>
#include "cppa/util/duration.hpp"

namespace cppa { namespace detail {

using boost::mutex;
using boost::thread;
using boost::lock_guard;
using boost::unique_lock;
using boost::condition_variable;

namespace this_thread { using namespace boost::this_thread; }

template<class Lock, class Condition>
inline bool wait_until(Lock& lock, Condition& cond,
                       const boost::system_time& timeout) {
    return cond.timed_wait(lock, timeout);
}

inline boost::system_time now() {
    return boost::get_system_time();
}

} } // namespace cppa::detail

inline boost::system_time& operator+=(boost::system_time& lhs,
                                      const cppa::util::duration& rhs) {
    switch (rhs.unit) {
        case cppa::util::time_unit::seconds:
            lhs += boost::posix_time::seconds(rhs.count);
            break;

        case cppa::util::time_unit::milliseconds:
            lhs += boost::posix_time::milliseconds(rhs.count);
            break;

        case cppa::util::time_unit::microseconds:
            lhs += boost::posix_time::microseconds(rhs.count);
            break;

        default: break;
    }
    return lhs;
}

#else

#define CPPA_USE_STD_THREADS

#include <mutex>
#include <thread>
#include <condition_variable>

namespace cppa { namespace detail {

using std::mutex;
using std::thread;
using std::lock_guard;
using std::unique_lock;
using std::condition_variable;

namespace this_thread { using namespace std::this_thread; }

// returns false if a timeout occured
template<class Lock, class Condition, typename TimePoint>
inline bool wait_until(Lock& lock, Condition& cond, const TimePoint& timeout) {
    return cond.wait_until(lock, timeout) != std::cv_status::timeout;
}

inline auto now() -> decltype(std::chrono::high_resolution_clock::now()) {
    return std::chrono::high_resolution_clock::now();
}

} } // namespace cppa::detail

#endif // __APPLE__

#endif // THREAD_HPP
