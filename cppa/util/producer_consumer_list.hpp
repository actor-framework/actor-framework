#ifndef PRODUCER_CONSUMER_LIST_HPP
#define PRODUCER_CONSUMER_LIST_HPP

#define CPPA_CACHE_LINE_SIZE 64

#include <atomic>
#include <cassert>
#include "cppa/detail/thread.hpp"

namespace cppa { namespace util {

// T is any type
template<typename T>
class producer_consumer_list
{

 public:

    typedef T* value_ptr;

    struct node
    {
        value_ptr value;
        std::atomic<node*> next;
        ~node() { delete value; }
        node(value_ptr val) : value(val), next(nullptr) { }
        char pad[CPPA_CACHE_LINE_SIZE - sizeof(value_ptr)- sizeof(std::atomic<node*>)];
    };

 private:

    // for one consumer at a time
    node* m_first;
    char m_pad1[CPPA_CACHE_LINE_SIZE - sizeof(node*)];

    // for one producers at a time
    node* m_last;
    char m_pad2[CPPA_CACHE_LINE_SIZE - sizeof(node*)];

    // shared among producers
    std::atomic<bool> m_consumer_lock;
    std::atomic<bool> m_producer_lock;

    void push_impl(node* tmp)
    {
        // acquire exclusivity
        while (m_producer_lock.exchange(true))
        {
            detail::this_thread::yield();
        }
        // publish & swing last forward
        m_last->next = tmp;
        m_last = tmp;
        // release exclusivity
        m_producer_lock = false;
    }

 public:

    producer_consumer_list()
    {
        m_first = m_last = new node(nullptr);
        m_consumer_lock = false;
        m_producer_lock = false;
    }

    ~producer_consumer_list()
    {
        while (m_first)
        {
            node* tmp = m_first;
            m_first = tmp->next;
            delete tmp;
        }
    }

    inline void push_back(value_ptr value)
    {
        assert(value != nullptr);
        push_impl(new node(value));
    }

    // returns nullptr on failure
    value_ptr try_pop()
    {
        value_ptr result = nullptr;
        while (m_consumer_lock.exchange(true))
        {
            detail::this_thread::yield();
        }
        // only one consumer allowed
        node* first = m_first;
        node* next = m_first->next;
        if (next)
        {
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
        else
        {
            // release exclusivity
            m_consumer_lock = false;
            return nullptr;
        }
    }

};

} } // namespace cppa::util

#endif // PRODUCER_CONSUMER_LIST_HPP
