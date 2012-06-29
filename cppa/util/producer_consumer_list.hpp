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


#ifndef CPPA_PRODUCER_CONSUMER_LIST_HPP
#define CPPA_PRODUCER_CONSUMER_LIST_HPP

#define CPPA_CACHE_LINE_SIZE 64

#include <chrono>
#include <thread>
#include <atomic>
#include <cassert>

// GCC hack
#if !defined(_GLIBCXX_USE_SCHED_YIELD) && !defined(__clang__)
#include <time.h>
namespace std { namespace this_thread { namespace {
inline void yield() noexcept {
    timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 1;
    nanosleep(&req, nullptr);
}
} } } // namespace std::this_thread::<anonymous>
#endif

// another GCC hack
#if !defined(_GLIBCXX_USE_NANOSLEEP) && !defined(__clang__)
#include <time.h>
namespace std { namespace this_thread { namespace {
template<typename Rep, typename Period>
inline void sleep_for(const chrono::duration<Rep, Period>& rt) {
    auto sec = chrono::duration_cast<chrono::seconds>(rt);
    auto nsec = chrono::duration_cast<chrono::nanoseconds>(rt - sec);
    timespec req;
    req.tv_sec = sec.count();
    req.tv_nsec = nsec.count();
    nanosleep(&req, nullptr);
}
} } } // namespace std::this_thread::<anonymous>
#endif

namespace cppa { namespace util {

/**
 * @brief A producer-consumer list.
 *
 * For implementation details see http://drdobbs.com/cpp/211601363.
 */
template<typename T>
class producer_consumer_list {

 public:

    typedef T                   value_type;
    typedef size_t              size_type;
    typedef ptrdiff_t           difference_type;
    typedef value_type&         reference;
    typedef const value_type&   const_reference;
    typedef value_type*         pointer;
    typedef const value_type*   const_pointer;

    struct node {
        pointer value;
        std::atomic<node*> next;
        node(pointer val) : value(val), next(nullptr) { }
        static constexpr size_type payload_size =
                sizeof(pointer) + sizeof(std::atomic<node*>);
        static constexpr size_type pad_size = (CPPA_CACHE_LINE_SIZE * ((payload_size / CPPA_CACHE_LINE_SIZE) + 1)) - payload_size;
        // avoid false sharing
        char pad[pad_size];

    };

 private:

    static_assert(sizeof(node*) < CPPA_CACHE_LINE_SIZE,
                  "sizeof(node*) >= CPPA_CACHE_LINE_SIZE");

    // for one consumer at a time
    node* m_first;
    char m_pad1[CPPA_CACHE_LINE_SIZE - sizeof(node*)];

    // for one producers at a time
    node* m_last;
    char m_pad2[CPPA_CACHE_LINE_SIZE - sizeof(node*)];

    // shared among producers
    std::atomic<bool> m_consumer_lock;
    std::atomic<bool> m_producer_lock;

    void push_impl(node* tmp) {
        // acquire exclusivity
        while (m_producer_lock.exchange(true)) {
            std::this_thread::yield();
        }
        // publish & swing last forward
        m_last->next = tmp;
        m_last = tmp;
        // release exclusivity
        m_producer_lock = false;
    }

 public:

    producer_consumer_list() {
        m_first = m_last = new node(nullptr);
        m_consumer_lock = false;
        m_producer_lock = false;
    }

    ~producer_consumer_list() {
        while (m_first) {
            node* tmp = m_first;
            m_first = tmp->next;
            delete tmp;
        }
    }

    inline void push_back(pointer value) {
        assert(value != nullptr);
        push_impl(new node(value));
    }

    // returns nullptr on failure
    pointer try_pop() {
        pointer result = nullptr;
        while (m_consumer_lock.exchange(true)) {
            std::this_thread::yield();
        }
        // only one consumer allowed
        node* first = m_first;
        node* next = m_first->next;
        if (next) {
            // queue is not empty
            result = next->value; // take it out of the node
            next->value = nullptr;
            // swing first forward
            m_first = next;
            // release exclusivity
            m_consumer_lock = false;
            // delete old dummy
            //first->value = nullptr;
            delete first;
            return result;
        }
        else {
            // release exclusivity
            m_consumer_lock = false;
            return nullptr;
        }
    }

};

} } // namespace cppa::util

#endif // CPPA_PRODUCER_CONSUMER_LIST_HPP
