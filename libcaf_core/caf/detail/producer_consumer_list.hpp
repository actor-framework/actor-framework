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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CAF_PRODUCER_CONSUMER_LIST_HPP
#define CAF_PRODUCER_CONSUMER_LIST_HPP

#include "caf/config.hpp"

#define CAF_CACHE_LINE_SIZE 64

#include <chrono>
#include <thread>
#include <atomic>
#include <cassert>

// GCC hack
#if !defined(_GLIBCXX_USE_SCHED_YIELD) && !defined(__clang__)
#include <time.h>
namespace std {
namespace this_thread {
namespace {
inline void yield() noexcept {
    timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 1;
    nanosleep(&req, nullptr);
}
}
}
} // namespace std::this_thread::<anonymous>
#endif

// another GCC hack
#if !defined(_GLIBCXX_USE_NANOSLEEP) && !defined(__clang__)
#include <time.h>
namespace std {
namespace this_thread {
namespace {
template<typename Rep, typename Period>
inline void sleep_for(const chrono::duration<Rep, Period>& rt) {
    auto sec = chrono::duration_cast<chrono::seconds>(rt);
    auto nsec = chrono::duration_cast<chrono::nanoseconds>(rt - sec);
    timespec req;
    req.tv_sec = sec.count();
    req.tv_nsec = nsec.count();
    nanosleep(&req, nullptr);
}
}
}
} // namespace std::this_thread::<anonymous>
#endif

namespace caf {
namespace detail {

/**
 * @brief A producer-consumer list.
 *
 * For implementation details see http://drdobbs.com/cpp/211601363.
 */
template<typename T>
class producer_consumer_list {

 public:

    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    class node {

     public:

        pointer value;

        std::atomic<node*> next;

        node(pointer val) : value(val), next(nullptr) {}

     private:

        static constexpr size_type payload_size =
            sizeof(pointer) + sizeof(std::atomic<node*>);

        static constexpr size_type cline_size = CAF_CACHE_LINE_SIZE;

        static constexpr size_type pad_size =
            (cline_size * ((payload_size / cline_size) + 1)) - payload_size;

        // avoid false sharing
        char pad[pad_size];

    };

 private:

    static_assert(sizeof(node*) < CAF_CACHE_LINE_SIZE,
                  "sizeof(node*) >= CAF_CACHE_LINE_SIZE");

    // for one consumer at a time
    std::atomic<node*> m_first;
    char m_pad1[CAF_CACHE_LINE_SIZE - sizeof(node*)];

    // for one producers at a time
    std::atomic<node*> m_last;
    char m_pad2[CAF_CACHE_LINE_SIZE - sizeof(node*)];

    // shared among producers
    std::atomic<bool> m_consumer_lock;
    std::atomic<bool> m_producer_lock;

 public:

    producer_consumer_list() {
        auto ptr = new node(nullptr);
        m_first = ptr;
        m_last = ptr;
        m_consumer_lock = false;
        m_producer_lock = false;
    }

    ~producer_consumer_list() {
        while (m_first) {
            node* tmp = m_first;
            m_first = tmp->next.load();
            delete tmp;
        }
    }

    inline void push_back(pointer value) {
        assert(value != nullptr);
        node* tmp = new node(value);
        // acquire exclusivity
        while (m_producer_lock.exchange(true)) {
            std::this_thread::yield();
        }
        // publish & swing last forward
        m_last.load()->next = tmp;
        m_last = tmp;
        // release exclusivity
        m_producer_lock = false;
    }

    // returns nullptr on failure
    pointer try_pop() {
        pointer result = nullptr;
        while (m_consumer_lock.exchange(true)) {
            std::this_thread::yield();
        }
        // only one consumer allowed
        node* first = m_first;
        node* next = m_first.load()->next;
        if (next) {
            // queue is not empty
            result = next->value; // take it out of the node
            next->value = nullptr;
            // swing first forward
            m_first = next;
            // release exclusivity
            m_consumer_lock = false;
            // delete old dummy
            // first->value = nullptr;
            delete first;
            return result;
        } else {
            // release exclusivity
            m_consumer_lock = false;
            return nullptr;
        }
    }

    bool empty() const {
        // atomically compares first and last pointer without locks
        return m_first == m_last;
    }

};

} // namespace detail
} // namespace caf

#endif // CAF_PRODUCER_CONSUMER_LIST_HPP
