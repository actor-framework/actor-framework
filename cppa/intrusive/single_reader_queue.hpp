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


#ifndef CPPA_SINGLE_READER_QUEUE_HPP
#define CPPA_SINGLE_READER_QUEUE_HPP

#include <list>
#include <mutex>
#include <atomic>
#include <memory>
#include <thread>
#include <condition_variable>

#include "cppa/config.hpp"

namespace cppa { namespace intrusive {

/**
 * @brief An intrusive, thread safe queue implementation.
 * @note For implementation details see
 *       http://libcppa.blogspot.com/2011/04/mailbox-part-1.html
 */
template<typename T>
class single_reader_queue {

    typedef std::unique_lock<std::mutex> lock_type;

 public:

    typedef T           value_type;
    typedef value_type* pointer;

    /**
     * @warning call only from the reader (owner)
     */
    pointer pop() {
        wait_for_data();
        return take_head();
    }

    /**
     * @warning call only from the reader (owner)
     */
    pointer try_pop() {
        return take_head();
    }

    /**
     * @warning call only from the reader (owner)
     */
    template<typename TimePoint>
    pointer try_pop(const TimePoint& abs_time) {
        return (timed_wait_for_data(abs_time)) ? take_head() : nullptr;
    }

    // returns true if the queue was empty
    bool _push_back(pointer new_element) {
        pointer e = m_stack.load();
        for (;;) {
            new_element->next = e;
            if (m_stack.compare_exchange_weak(e, new_element)) {
                return (e == nullptr);
            }
        }
    }

    void push_back(pointer new_element) {
        pointer e = m_stack.load();
        for (;;) {
            new_element->next = e;
            if (!e) {
                lock_type guard(m_mtx);
                if (m_stack.compare_exchange_weak(e, new_element)) {
                    m_cv.notify_one();
                    return;
                }
            }
            else {
                if (m_stack.compare_exchange_weak(e, new_element)) {
                    return;
                }
            }
        }
    }

    inline bool can_fetch_more() const {
        return m_stack.load() != nullptr;
    }

    /**
     * @warning call only from the reader (owner)
     */
    inline bool empty() const {
        return m_head == nullptr && m_stack.load() == nullptr;
    }

    /**
     * @warning call only from the reader (owner)
     */
    inline bool not_empty() const {
        return !empty();
    }

    single_reader_queue() : m_stack(nullptr), m_head(nullptr) {
    }

    ~single_reader_queue() {
        // empty the stack (void) fetch_new_data();
    }

 private:

    // exposed to "outside" access
    std::atomic<pointer> m_stack;

    // accessed only by the owner
    pointer m_head;

    // locked on enqueue/dequeue operations to/from an empty list
    std::mutex m_mtx;
    std::condition_variable m_cv;

    template<typename TimePoint>
    bool timed_wait_for_data(const TimePoint& timeout) {
        if (empty()) {
            lock_type guard(m_mtx);
            while (m_stack.load() == nullptr) {
                if (m_cv.wait_until(guard, timeout) == std::cv_status::timeout) {
                    return false;
                }
            }
        }
        return true;
    }

    void wait_for_data() {
        if (empty()) {
            lock_type guard(m_mtx);
            while (!(m_stack.load())) m_cv.wait(guard);
        }
    }

    // atomically sets m_stack to nullptr and enqueues all elements to the cache
    bool fetch_new_data() {
        CPPA_REQUIRE(m_head == nullptr);
        pointer e = m_stack.load();
        while (e) {
            if (m_stack.compare_exchange_weak(e, 0)) {
                while (e) {
                    auto next = e->next;
                    e->next = m_head;
                    m_head = e;
                    e = next;
                }
                return true;
            }
            // next iteration
        }
        // !public_tail
        return false;
    }

    pointer take_head() {
        if (m_head != nullptr || fetch_new_data()) {
            auto result = m_head;
            m_head = m_head->next;
            return result;
        }
        return nullptr;
    }

};

} } // namespace cppa::util

#endif // CPPA_SINGLE_READER_QUEUE_HPP
