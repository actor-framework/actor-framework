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


#ifndef CPPA_BLOCKING_SINGLE_READER_QUEUE_HPP
#define CPPA_BLOCKING_SINGLE_READER_QUEUE_HPP

#include <mutex>
#include <thread>
#include <condition_variable>

#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { namespace intrusive {

template<typename T, class Delete = std::default_delete<T> >
class blocking_single_reader_queue {

    typedef std::unique_lock<std::mutex> lock_type;

 public:

    typedef single_reader_queue<T,Delete>  impl_type;
    typedef typename impl_type::value_type value_type;
    typedef typename impl_type::pointer    pointer;

    pointer pop() {
        wait_for_data();
        return m_impl.try_pop();
    }

    inline pointer try_pop() {
        return m_impl.try_pop();
    }

    /**
     * @warning call only from the reader (owner)
     */
    template<typename TimePoint>
    pointer try_pop(const TimePoint& abs_time) {
        return (timed_wait_for_data(abs_time)) ? try_pop() : nullptr;
    }

    void push_back(pointer new_element) {
        if (m_impl.enqueue(new_element) == first_enqueued) {
            lock_type guard(m_mtx);
            m_cv.notify_one();
        }
    }

    inline void clear() {
        m_impl.clear();
    }

    inline void close() {
        m_impl.close();
    }

    template<typename F>
    inline void close(const F& f) {
        m_impl.close(f);
    }

 private:

    // locked on enqueue/dequeue operations to/from an empty list
    std::mutex m_mtx;
    std::condition_variable m_cv;
    impl_type m_impl;

    template<typename TimePoint>
    bool timed_wait_for_data(const TimePoint& timeout) {
        CPPA_REQUIRE(!m_impl.closed());
        if (m_impl.empty()) {
            lock_type guard(m_mtx);
            while (m_impl.empty()) {
                if (m_cv.wait_until(guard, timeout) == std::cv_status::timeout) {
                    return false;
                }
            }
        }
        return true;
    }

    void wait_for_data() {
        if (m_impl.empty()) {
            lock_type guard(m_mtx);
            while (m_impl.empty()) m_cv.wait(guard);
        }
    }

};

} } // namespace cppa::intrusive

#endif // CPPA_BLOCKING_SINGLE_READER_QUEUE_HPP
