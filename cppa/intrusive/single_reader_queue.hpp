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

#include <list>
#include <atomic>
#include <memory>

#include "cppa/config.hpp"
#include "cppa/detail/thread.hpp"

namespace cppa { namespace intrusive {

template<typename List>
struct default_list_append
{
    template<typename T>
    typename List::iterator operator()(List& l, T* e)
    {
        CPPA_REQUIRE(e != nullptr);
        // temporary list to convert LIFO to FIFO order
        List tmp;
        // public_tail (e) has LIFO order,
        // but private_head requires FIFO order
        while (e)
        {
            // next iteration element
            T* next = e->next;
            // insert e to private cache (convert to LIFO order)
            tmp.emplace_front(e);
            e = next;
        }
        CPPA_REQUIRE(tmp.empty() == false);
        auto result = tmp.begin();
        l.splice(l.end(), tmp);
        return result;
    }

};

/**
 * @brief An intrusive, thread safe queue implementation.
 * @note For implementation details see
 *       http://libcppa.blogspot.com/2011/04/mailbox-part-1.html
 */
template<typename T,
         class CacheType = std::list<std::unique_ptr<T> >,
         class CacheAppend = default_list_append<std::list<std::unique_ptr<T> > > >
class single_reader_queue
{

    typedef detail::unique_lock<detail::mutex> lock_type;

 public:

    typedef T           value_type;
    typedef value_type* pointer;

    typedef CacheType                            cache_type;
    typedef typename cache_type::value_type      cache_value_type;
    typedef typename cache_type::iterator        cache_iterator;

    /**
     * @warning call only from the reader (owner)
     */
    cache_value_type pop()
    {
        wait_for_data();
        cache_value_type result;
        take_head(result);
        return result;
    }

    /**
     * @warning call only from the reader (owner)
     */
    bool try_pop(cache_value_type& result)
    {
        return take_head(result);
    }

    /**
     * @warning call only from the reader (owner)
     */
    template<typename TimePoint>
    bool try_pop(cache_value_type& result, TimePoint const& abs_time)
    {
        return (timed_wait_for_data(abs_time)) ? take_head(result) : false;
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

    inline bool can_fetch_more() const
    {
        return m_stack.load() != nullptr;
    }

    /**
     * @warning call only from the reader (owner)
     */
    inline bool empty() const
    {
        return m_cache.empty() && m_stack.load() == nullptr;
    }

    /**
     * @warning call only from the reader (owner)
     */
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

    cache_iterator try_fetch_more()
    {
        cache_iterator result = m_cache.end();
        fetch_new_data(&result);
        return result;
    }

    template<typename TimePoint>
    cache_iterator try_fetch_more(TimePoint const& abs_time)
    {
        cache_iterator result = m_cache.end();
        if (timed_wait_for_data(abs_time)) fetch_new_data(&result);
        return result;
    }

    cache_iterator fetch_more()
    {
        cache_iterator result = m_cache.end();
        wait_for_data();
        fetch_new_data(&result);
        return result;
    }

 private:

    // exposed to "outside" access
    std::atomic<pointer> m_stack;

    // accessed only by the owner
    cache_type m_cache;
    CacheAppend m_append;

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
    bool fetch_new_data(cache_iterator* iter = nullptr)
    {
        pointer e = m_stack.load();
        while (e)
        {
            if (m_stack.compare_exchange_weak(e, 0))
            {
                auto i = m_append(m_cache, e);
                if (iter) *iter = i;
                return true;
            }
            // next iteration
        }
        // !public_tail
        return false;
    }

    bool take_head(cache_value_type& result)
    {
        if (!m_cache.empty() || fetch_new_data())
        {
            result = std::move(m_cache.front());
            m_cache.pop_front();
            return true;
        }
        return false;
    }

};

} } // namespace cppa::util

#endif // SINGLE_READER_QUEUE_HPP
