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
#include <atomic>
#include <memory>

#include "cppa/config.hpp"

namespace cppa { namespace intrusive {

enum enqueue_result { enqueued, first_enqueued, queue_closed };

/**
 * @brief An intrusive, thread safe queue implementation.
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
            if (e == nullptr) {
                m_delete(new_element);
                return queue_closed; // queue is closed
            }
            new_element->next = e;
            if (m_stack.compare_exchange_weak(e, new_element)) {
                return (e == stack_end()) ? first_enqueued : enqueued;
            }
        }
    }

    inline bool can_fetch_more() const {
        return m_stack.load() != stack_end();
    }

    /**
     * @warning call only from the reader (owner)
     */
    inline bool empty() const {
        return closed() || (m_head == nullptr && m_stack.load() == stack_end());
    }

    inline bool closed() const {
        return m_stack.load() == nullptr;
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
        m_stack = stack_end();
    }

    inline void clear() {
        if (!closed()) {
            clear_cached_elements();
            if (fetch_new_data()) clear_cached_elements();
        }
    }

    inline ~single_reader_queue() { clear(); }

 private:

    // exposed to "outside" access
    std::atomic<pointer> m_stack;

    // accessed only by the owner
    pointer m_head;
    Delete  m_delete;

    // atomically sets m_stack back and enqueues all elements to the cache
    bool fetch_new_data(pointer end_ptr) {
        CPPA_REQUIRE(m_head == nullptr);
        CPPA_REQUIRE(end_ptr == nullptr || end_ptr == stack_end());
        pointer e = m_stack.load();
        // it's enough to check this once, since only the owner is allowed
        // to close the queue and only the owner is allowed to call this
        // member function
        if (e == nullptr) return false;
        while (e != end_ptr) {
            if (m_stack.compare_exchange_weak(e, end_ptr)) {
                while (e != stack_end()) {
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
        return fetch_new_data(stack_end());
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
        while (m_head != nullptr) {
            auto next = m_head->next;
            f(*m_head);
            m_delete(m_head);
            m_head = next;
        }
    }

    pointer stack_end() const {
        // we are *never* going to dereference the returned pointer;
        // it is only used as indicator wheter this queue is closed or not
        return reinterpret_cast<pointer>(const_cast<single_reader_queue*>(this));
    }

};

} } // namespace cppa::util

#endif // CPPA_SINGLE_READER_QUEUE_HPP
