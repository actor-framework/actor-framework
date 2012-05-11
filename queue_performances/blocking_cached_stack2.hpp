#ifndef BLOCKING_CACHED_STACK2_HPP
#define BLOCKING_CACHED_STACK2_HPP

#include <atomic>
#include <boost/thread.hpp>

#include "defines.hpp"

// This class is intrusive.
template<typename T>
class blocking_cached_stack2 {

    // singly linked list, serves as cache
    T* m_head;
    char m_pad1[CACHE_LINE_SIZE - sizeof(T*)];

    // modified by consumers
    std::atomic<T*> m_stack;
    char m_pad2[CACHE_LINE_SIZE - sizeof(std::atomic<T*>)];

    T* m_dummy;
    char m_pad3[CACHE_LINE_SIZE - sizeof(T)];

    // locked on enqueue/dequeue operations to/from an empty list
    std::mutex m_mtx;
    std::condition_variable m_cv;

    typedef std::unique_lock<std::mutex> lock_type;

    // read all elements of m_stack, convert them to FIFO order and store
    // them in m_head
    // precondition: m_head == nullptr
    void consume_stack() {
        T* e = m_stack.load();
        while (e) {
            // enqueue dummy instead of nullptr to reduce
            // lock operations
            if (m_stack.compare_exchange_weak(e, m_dummy)) {
                // m_stack is now empty (m_stack == m_dummy)
                // m_dummy marks always the end of the stack
                while (e && e != m_dummy) {
                    T* next = e->next;
                    // enqueue to m_head
                    e->next = m_head;
                    m_head = e;
                    // next iteration
                    e = next;
                }
                return;
            }
        }
        // nothing to consume
    }

    void wait_for_data() {
        if (!m_head) {
            T* e = m_stack.load();
            while (e == m_dummy) {
                if (m_stack.compare_exchange_weak(e, 0)) e = 0;
            }
            if (!e) {
                lock_type lock(m_mtx);
                while (!(m_stack.load())) m_cv.wait(lock);
            }
            consume_stack();
        }
    }

    void delete_head() {
        while (m_head) {
            T* next = m_head->next;
            delete m_head;
            m_head = next;
        }

    }

 public:

    blocking_cached_stack2() : m_head(0) {
        m_stack = 0;
        m_dummy = new T;
    }

    ~blocking_cached_stack2() {
        delete_head();
        T* e = m_stack.load();
        if (e && e != m_dummy) {
            consume_stack();
            delete_head();
        }
        delete m_dummy;
    }

    void push(T* what) {
        T* e = m_stack.load();
        for (;;) {
            what->next = e;
            if (!e) {
                lock_type lock(m_mtx);
                if (m_stack.compare_exchange_weak(e, what)) {
                    m_cv.notify_one();
                    return;
                }
            }
            // compare_exchange_weak stores the
            // new value to e if the operation fails
            else if (m_stack.compare_exchange_weak(e, what)) return;
        }
    }

    T* pop() {
        wait_for_data();
        T* result = m_head;
        m_head = m_head->next;
        return result;
    }

};

#endif // BLOCKING_CACHED_STACK2_HPP
