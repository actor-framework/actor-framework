#ifndef SINGLE_READER_QUEUE_HPP
#define SINGLE_READER_QUEUE_HPP

#include <atomic>
#include <boost/thread.hpp>

namespace cppa { namespace util {

/**
 * @brief An intrusive, thread safe queue implementation.
 */
template<typename T>
class single_reader_queue
{

    typedef boost::unique_lock<boost::mutex> lock_type;

 public:

    typedef T element_type;

    /**
     * @warning call only from the reader (owner)
     */
    element_type* pop()
    {
        wait_for_data();
        element_type* result = take_head();
        return result;
    }

    /**
     * @warning call only from the reader (owner)
     */
    element_type* try_pop()
    {
        return take_head();
    }

    element_type* try_pop(unsigned long ms_timeout)
    {
        boost::system_time st = boost::get_system_time();
        st += boost::posix_time::milliseconds(ms_timeout);
        if (timed_wait_for_data(st))
        {
            return try_pop();
        }
        return nullptr;
    }

    //element_type* peek()
    //{
    //    return (m_head || fetch_new_data()) ? m_head : nullptr;
    //}

    /**
     * @warning call only from the reader (owner)
     */
    void push_front(element_type* element)
    {
        element->next = m_head;
        m_head = element;
    }

    /**
     * @warning call only from the reader (owner)
     */
    void push_front(element_type* first, element_type* last)
    {
        last->next = m_head;
        m_head = first;
    }

    /**
     * @warning call only from the reader (owner)
     */
    template<typename List>
    void push_front(List&& list)
    {
        if (!list.empty())
        {
            auto p = list.take();
            if (p.first)
            {
                push_front(p.first, p.second);
            }
        }
    }

    // returns true if the queue was empty
    bool _push_back(element_type* new_element)
    {
        element_type* e = m_tail.load();
        for (;;)
        {
            new_element->next = e;
            if (!e)
            {
                if (m_tail.compare_exchange_weak(e, new_element))
                {
                    return true;
                }
            }
            else
            {
                if (m_tail.compare_exchange_weak(e, new_element))
                {
                    return false;
                }
            }
        }
    }

    /**
     * @reentrant
     */
    void push_back(element_type* new_element)
    {
        element_type* e = m_tail.load();
        for (;;)
        {
            new_element->next = e;
            if (!e)
            {
                lock_type guard(m_mtx);
                if (m_tail.compare_exchange_weak(e, new_element))
                {
                    m_cv.notify_one();
                    return;
                }
            }
            else
            {
                if (m_tail.compare_exchange_weak(e, new_element))
                {
                    return;
                }
            }
        }
    }

    /**
     * @warning call only from the reader (owner)
     */
    bool empty()
    {
        return !m_head && !(m_tail.load());
    }

    single_reader_queue() : m_tail(nullptr), m_head(nullptr)
    {
    }

    ~single_reader_queue()
    {
        fetch_new_data();
        element_type* e = m_head;
        while (e)
        {
            element_type* tmp = e;
            e = e->next;
            delete tmp;
        }
    }

 private:

    // exposed to "outside" access
    std::atomic<element_type*> m_tail;

    // accessed only by the owner
    element_type* m_head;

    // locked on enqueue/dequeue operations to/from an empty list
    boost::mutex m_mtx;
    boost::condition_variable m_cv;

    bool timed_wait_for_data(const boost::system_time& timeout)
    {
        if (!m_head && !(m_tail.load()))
        {
            lock_type guard(m_mtx);
            while (!(m_tail.load()))
            {
                if (!m_cv.timed_wait(guard, timeout))
                {
                    return false;
                }
            }
        }
        return true;
    }

    void wait_for_data()
    {
        if (!m_head && !(m_tail.load()))
        {
            lock_type guard(m_mtx);
            while (!(m_tail.load())) m_cv.wait(guard);
        }
    }

    // atomically set public_tail to nullptr and enqueue all
    bool fetch_new_data()
    {
        element_type* e = m_tail.load();
        while (e)
        {
            if (m_tail.compare_exchange_weak(e, 0))
            {
                // public_tail (e) has LIFO order,
                // but private_head requires FIFO order
                while (e)
                {
                    // next iteration element
                    element_type* next = e->next;
                    // enqueue e to private_head
                    e->next = m_head;
                    m_head = e;
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

    element_type* take_head()
    {
        if (m_head || fetch_new_data())
        {
            element_type* result = m_head;
            m_head = result->next;
            return result;
        }
        return 0;
    }

};

} } // namespace cppa::util

#endif // SINGLE_READER_QUEUE_HPP
