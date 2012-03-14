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


#ifndef SINGLE_READER_QUEUE_HPP
#define SINGLE_READER_QUEUE_HPP

#include <atomic>

#include "cppa/detail/thread.hpp"
#include "cppa/intrusive/singly_linked_list.hpp"

namespace cppa { namespace intrusive {

/**
 * @brief An intrusive, thread safe queue implementation.
 * @note For implementation details see
 *       http://libcppa.blogspot.com/2011/04/mailbox-part-1.html
 */
template<typename T>
class single_reader_queue
{

    typedef detail::unique_lock<detail::mutex> lock_type;

 public:

    typedef T                   value_type;
    typedef size_t              size_type;
    typedef ptrdiff_t           difference_type;
    typedef value_type&         reference;
    typedef value_type const&   const_reference;
    typedef value_type*         pointer;
    typedef value_type const*   const_pointer;

    typedef singly_linked_list<value_type> cache_type;

    /**
     * @warning call only from the reader (owner)
     */
    pointer pop()
    {
        wait_for_data();
        return take_head();
    }

    /**
     * @warning call only from the reader (owner)
     */
    pointer try_pop()
    {
        return take_head();
    }

    /**
     * @warning call only from the reader (owner)
     */
    template<typename TimePoint>
    pointer try_pop(TimePoint const& abs_time)
    {
        return (timed_wait_for_data(abs_time)) ? take_head() : nullptr;
    }

    /**
     * @warning call only from the reader (owner)
     */
    void push_front(pointer element)
    {
        m_cache.push_front(element);
    }

    /**
     * @warning call only from the reader (owner)
     */
    void push_front(cache_type&& list)
    {
        m_cache.splice_after(m_cache.before_begin(), std::move(list));
    }

    // returns true if the queue was empty
    bool _push_back(pointer new_element)
    {
        pointer e = m_stack.load();
        for (;;)
        {
            new_element->next = e;
            if (m_stack.compare_exchange_weak(e, new_element))
            {
                return (e == nullptr);
            }
        }
    }

    /**
     * @reentrant
     */
    void push_back(pointer new_element)
    {
        pointer e = m_stack.load();
        for (;;)
        {
            new_element->next = e;
            if (!e)
            {
                lock_type guard(m_mtx);
                if (m_stack.compare_exchange_weak(e, new_element))
                {
                    m_cv.notify_one();
                    return;
                }
            }
            else
            {
                if (m_stack.compare_exchange_weak(e, new_element))
                {
                    return;
                }
            }
        }
    }

    inline cache_type& cache() { return m_cache; }

    /**
     * @warning call only from the reader (owner)
     */
    inline bool empty() const
    {
        return m_cache.empty() && m_stack.load() == nullptr;
    }

    inline bool not_empty() const
    {
        return !empty();
    }

    single_reader_queue() : m_stack(nullptr)
    {
    }

    ~single_reader_queue()
    {
        // empty the stack
        (void) fetch_new_data();
    }

 private:

    // exposed to "outside" access
    std::atomic<pointer> m_stack;

    // accessed only by the owner
    cache_type m_cache;

    // locked on enqueue/dequeue operations to/from an empty list
    detail::mutex m_mtx;
    detail::condition_variable m_cv;

    template<typename TimePoint>
    bool timed_wait_for_data(TimePoint const& timeout)
    {
        if (m_cache.empty() && !(m_stack.load()))
        {
            lock_type guard(m_mtx);
            while (!(m_stack.load()))
            {
                if (detail::wait_until(guard, m_cv, timeout) == false)
                {
                    return false;
                }
            }
        }
        return true;
    }

    void wait_for_data()
    {
        if (m_cache.empty() && !(m_stack.load()))
        {
            lock_type guard(m_mtx);
            while (!(m_stack.load())) m_cv.wait(guard);
        }
    }

    // atomically sets m_stack to nullptr and enqueues all elements to the cache
    bool fetch_new_data()
    {
        pointer e = m_stack.load();
        while (e)
        {
            if (m_stack.compare_exchange_weak(e, 0))
            {
                // iterator to the last element before insertions begin
                auto iter = m_cache.before_end();
                // public_tail (e) has LIFO order,
                // but private_head requires FIFO order
                while (e)
                {
                    // next iteration element
                    pointer next = e->next;
                    // insert e to private cache (convert to LIFO order)
                    m_cache.insert_after(iter, e);
                    // next iteration
                    e = next;
                }
                return true;
            }
            // next iteration
        }
        // !public_tail
        return false;
    }

    pointer take_head()
    {
        if (m_cache.not_empty() || fetch_new_data())
        {
            return m_cache.take_after(m_cache.before_begin());
        }
        return nullptr;
    }

};

} } // namespace cppa::util

#endif // SINGLE_READER_QUEUE_HPP
