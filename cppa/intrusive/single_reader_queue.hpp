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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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
#include <condition_variable> // std::cv_status

#include "cppa/config.hpp"

namespace cppa {
namespace intrusive {

/**
 * @brief Denotes in which state queue and reader are after an enqueue.
 */
enum class enqueue_result {

    /**
     * @brief Indicates that the enqueue operation succeeded and
     *        the reader is ready to receive the data.
     **/
    success,

    /**
     * @brief Indicates that the enqueue operation succeeded and
     *        the reader is currently blocked, i.e., needs to be re-scheduled.
     **/
    unblocked_reader,

    /**
     * @brief Indicates that the enqueue operation failed because the
     *        queue has been closed by the reader.
     **/
    queue_closed

};

/**
 * @brief An intrusive, thread-safe queue implementation.
 * @note For implementation details see
 *       http://libcppa.blogspot.com/2011/04/mailbox-part-1.html
 */
template<typename T, class Delete = std::default_delete<T> >
class single_reader_queue {

 public:

    typedef T           value_type;
    typedef value_type* pointer;

    /**
     * @warning call only from the reader (owner)
     */
    pointer try_pop() {
        return take_head();
    }

    template<class UnaryPredicate>
    void remove_if(UnaryPredicate f) {
        pointer head = m_head;
        pointer last = nullptr;
        pointer p = m_head;
        auto loop = [&]() -> bool {
            while (p) {
                if (f(*p)) {
                    if (last == nullptr) m_head = p->next;
                    else last = p->next;
                    m_delete(p);
                    return true;
                }
                else {
                    last = p;
                    p = p->next;
                }
            }
            return false;
        };
        if (!loop()) {
            // last points to the tail now
            auto old_tail = last;
            m_head = nullptr; // fetch_new_data assumes cached list to be empty
            if (fetch_new_data()) {
                last = nullptr;
                p = m_head; // let p point to the first newly fetched element
                loop();
                // restore cached list
                if (head) {
                    old_tail->next = m_head;
                    m_head = head;
                }
            }
            else m_head = head;
        }
    }

    // returns true if the queue was empty
    enqueue_result enqueue(pointer new_element) {
        pointer e = m_stack.load();
        for (;;) {
            if (!e) {
                // if tail is nullptr, the queue has been closed
                m_delete(new_element);
                return enqueue_result::queue_closed;
            }
            new_element->next = is_dummy(e) ? nullptr : e;
            if (m_stack.compare_exchange_weak(e, new_element)) {
                return (e == reader_blocked_dummy())
                        ? enqueue_result::unblocked_reader
                        : enqueue_result::success;
            }
        }
    }

    /**
     * @brief Queries whether there is new data to read.
     * @pre m_stack.load() != reader_blocked_dummy()
     */
    inline bool can_fetch_more() {
        auto ptr = m_stack.load();
        CPPA_REQUIRE(ptr != nullptr);
        return !is_dummy(ptr);
    }

    /**
     * @warning call only from the reader (owner)
     */
    inline bool empty() {
        CPPA_REQUIRE(m_stack.load() != nullptr);
        return (!m_head && is_dummy(m_stack.load()));
    }

    inline bool closed() {
        return m_stack.load() == nullptr;
    }

    inline bool blocked() {
        return m_stack == reader_blocked_dummy();
    }

    /**
     * @brief Tries to set this queue from state @p empty to state @p blocked.
     * @returns @p true if the state change was successful or if the mailbox
     *          was already blocked, otherwise @p false.
     * @note This function does never fail spuriously.
     */
    inline bool try_block() {
        auto e = stack_empty_dummy();
        bool res = m_stack.compare_exchange_strong(e, reader_blocked_dummy());
        // return true in case queue was already blocked
        return res || e == reader_blocked_dummy();
    }

    /**
     * @brief Tries to set this queue from state @p blocked to state @p empty.
     * @returns @p true if the state change was successful, otherwise @p false.
     * @note This function does never fail spuriously.
     */
    inline bool try_unblock() {
        auto e = reader_blocked_dummy();
        return m_stack.compare_exchange_strong(e, stack_empty_dummy());
    }

    /**
     * @warning call only from the reader (owner)
     */
    // closes this queue deletes all remaining elements
    inline void close() {
        clear_cached_elements();
        if (fetch_new_data(nullptr)) clear_cached_elements();
    }

    // closes this queue and applies f to all remaining elements before deleting
    template<typename F>
    inline void close(const F& f) {
        clear_cached_elements(f);
        if (fetch_new_data(nullptr)) clear_cached_elements(f);
    }

    inline single_reader_queue() : m_head(nullptr) {
        m_stack = stack_empty_dummy();
    }

    inline void clear() {
        if (!closed()) {
            clear_cached_elements();
            if (fetch_new_data()) clear_cached_elements();
        }
    }

    ~single_reader_queue() {
        clear();
    }

    /**************************************************************************
     *                    support for synchronized access                     *
     **************************************************************************/

    template<class Mutex, class CondVar>
    bool synchronized_enqueue(Mutex& mtx, CondVar& cv, pointer new_element) {
        switch (enqueue(new_element)) {
            case enqueue_result::unblocked_reader: {
                std::unique_lock<Mutex> guard(mtx);
                cv.notify_one();
                return true;
            }
            case enqueue_result::success:
                // enqueued message to a running actor's mailbox
                return true;
            case enqueue_result::queue_closed:
                // actor no longer alive
                return false;
        }
        // should be unreachable
        CPPA_CRITICAL("invalid result of enqueue()");
    }

    template<class Mutex, class CondVar, class TimePoint>
    pointer synchronized_try_pop(Mutex& mtx, CondVar& cv, const TimePoint& abs_time) {
        auto res = try_pop();
        if (!res && synchronized_await(mtx, cv, abs_time)) {
            res = try_pop();
        }
        return res;
    }

    template<class Mutex, class CondVar>
    pointer synchronized_pop(Mutex& mtx, CondVar& cv) {
        auto res = try_pop();
        if (!res) {
            synchronized_await(mtx, cv);
            res = try_pop();
        }
        return res;
    }

    template<class Mutex, class CondVar>
    void synchronized_await(Mutex& mtx, CondVar& cv) {
        CPPA_REQUIRE(!closed());
        if (try_block()) {
            std::unique_lock<Mutex> guard(mtx);
            while (blocked()) cv.wait(guard);
        }
    }

    template<class Mutex, class CondVar, class TimePoint>
    bool synchronized_await(Mutex& mtx, CondVar& cv, const TimePoint& timeout) {
        CPPA_REQUIRE(!closed());
        if (try_block()) {
            std::unique_lock<Mutex> guard(mtx);
            while (blocked()) {
                if (cv.wait_until(guard, timeout) == std::cv_status::timeout) {
                    // if we're unable to set the queue from blocked to empty,
                    // than there's a new element in the list
                    return !try_unblock();
                }
            }
        }
        return true;
    }

 private:

    // exposed to "outside" access
    std::atomic<pointer> m_stack;

    // accessed only by the owner
    pointer m_head;
    Delete  m_delete;

    // atomically sets m_stack back and enqueues all elements to the cache
    bool fetch_new_data(pointer end_ptr) {
        CPPA_REQUIRE(m_head == nullptr);
        CPPA_REQUIRE(!end_ptr || end_ptr == stack_empty_dummy());
        pointer e = m_stack.load();
        // must not be called on a closed queue
        CPPA_REQUIRE(e != nullptr);
        // it's enough to check this once, since only the owner is allowed
        // to close the queue and only the owner is allowed to call this
        // member function
        while (e != end_ptr) {
            if (m_stack.compare_exchange_weak(e, end_ptr)) {
                if (is_dummy(e)) {
                    // only use-case for this is closing a queue
                    CPPA_REQUIRE(end_ptr == nullptr);
                    return false;
                }
                while (e) {
                    CPPA_REQUIRE(!is_dummy(e));
                    auto next = e->next;
                    e->next = m_head;
                    m_head = e;
                    e = next;
                }
                return true;
            }
            // next iteration
        }
        return false;
    }

    inline bool fetch_new_data() {
        return fetch_new_data(stack_empty_dummy());
    }

    pointer take_head() {
        if (m_head != nullptr || fetch_new_data()) {
            auto result = m_head;
            m_head = m_head->next;
            return result;
        }
        return nullptr;
    }

    void clear_cached_elements() {
        while (m_head != nullptr) {
            auto next = m_head->next;
            m_delete(m_head);
            m_head = next;
        }
    }

    template<typename F>
    void clear_cached_elements(const F& f) {
        while (m_head) {
            auto next = m_head->next;
            f(*m_head);
            m_delete(m_head);
            m_head = next;
        }
    }

    inline pointer stack_empty_dummy() {
        // we are *never* going to dereference the returned pointer;
        // it is only used as indicator wheter this queue is closed or not
        return reinterpret_cast<pointer>(this);
    }

    inline pointer reader_blocked_dummy() {
        // we are not going to dereference this pointer either
        return reinterpret_cast<pointer>(reinterpret_cast<std::intptr_t>(this) + sizeof(void*));
    }

    inline bool is_dummy(pointer ptr) {
        return ptr == stack_empty_dummy() || ptr == reader_blocked_dummy();
    }

};

} // namespace intrusive
} // namespace cppa

#endif // CPPA_SINGLE_READER_QUEUE_HPP
