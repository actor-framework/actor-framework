#ifndef BLOCKING_SUTTER_LIST_HPP
#define BLOCKING_SUTTER_LIST_HPP

#include <atomic>
#include <boost/thread.hpp>

#include "defines.hpp"

// This implementation is a single-consumer version
// of an example implementation of Herb Sutter:
// http://drdobbs.com/cpp/211601363.

// T is any type
template<typename T>
class blocking_sutter_list {

    struct node {
        node(T* val = 0) : value(val), next(0) { }
        T* value;
        std::atomic<node*> next;
        char pad[CACHE_LINE_SIZE - sizeof(T*)- sizeof(std::atomic<node*>)];
    };

    // one consumer at a time
    node* m_first;
    char m_pad1[CACHE_LINE_SIZE - sizeof(node*)];

    // for one producers at a time
    node* m_last;
    char m_pad2[CACHE_LINE_SIZE - sizeof(node*)];

    // shared among producers
    std::atomic<bool> m_producer_lock;
    char m_pad3[CACHE_LINE_SIZE - sizeof(std::atomic<bool>)];

    // locked on enqueue/dequeue operations to/from an empty list
    std::mutex m_mtx;
    std::condition_variable m_cv;

    typedef std::unique_lock<std::mutex> lock_type;

 public:

    blocking_sutter_list() {
        m_first = m_last = new node;
        m_producer_lock = false;
    }

    ~blocking_sutter_list() {
        while (m_first) {
            node* tmp = m_first;
            m_first = tmp->next;
            delete tmp;
        }
    }

    // takes ownership of what
    void push(T* what) {
        bool consumer_might_sleep = 0;
        node* tmp = new node(what);
        // acquire exclusivity
        while (m_producer_lock.exchange(true)) {
            std::this_thread::yield();
        }
        // do we have to wakeup a sleeping consumer?
        // this is a sufficient condition because m_last->value is 0
        // if and only if m_head == m_tail
        consumer_might_sleep = (m_last->value == 0);
        // publish & swing last forward
        m_last->next = tmp;
        m_last = tmp;
        // release exclusivity
        m_producer_lock = false;
        // wakeup consumer if needed
        if (consumer_might_sleep) {
            lock_type lock(m_mtx);
            m_cv.notify_one();
        }
    }

    // polls the queue until an element was dequeued
    T* pop() {
        node* first = m_first;
        node* next = m_first->next;
        if (!next) {
            lock_type lock(m_mtx);
            while (!(next = m_first->next)) {
                m_cv.wait(lock);
            }
        }
        T* result = next->value; // take it out
        next->value = 0;         // of the node
        // swing first forward
        m_first = next;
        // delete old dummy
        delete first;
        // done
        return result;
    }

};

#endif // BLOCKING_SUTTER_LIST_HPP
