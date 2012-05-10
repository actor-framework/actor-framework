#ifndef PRODUCER_CONSUMER_LIST_HPP
#define PRODUCER_CONSUMER_LIST_HPP

#define CPPA_CACHE_LINE_SIZE 64

#include <atomic>
#include <cassert>
#include "cppa/detail/thread.hpp"

namespace cppa { namespace util {

/**
 * @brief A producer-consumer list.
 *
 * For implementation details see http://drdobbs.com/cpp/211601363.
 */
template<typename T>
class producer_consumer_list
{

 public:

    typedef T                   value_type;
    typedef size_t              size_type;
    typedef ptrdiff_t           difference_type;
    typedef value_type&         reference;
    typedef value_type const&   const_reference;
    typedef value_type*         pointer;
    typedef value_type const*   const_pointer;

    struct node
    {
        pointer value;
        std::atomic<node*> next;
        node(pointer val) : value(val), next(nullptr) { }
        static constexpr size_type payload_size =
                sizeof(pointer) + sizeof(std::atomic<node*>);
        static constexpr size_type pad_size =
                (CPPA_CACHE_LINE_SIZE * ((payload_size / CPPA_CACHE_LINE_SIZE) + 1)) - payload_size;
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

    inline void push_back(pointer value)
    {
        assert(value != nullptr);
        push_impl(new node(value));
    }

    // returns nullptr on failure
    pointer try_pop()
    {
        pointer result = nullptr;
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
