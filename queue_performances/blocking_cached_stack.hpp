#ifndef BLOCKING_CACHED_STACK_HPP
#define BLOCKING_CACHED_STACK_HPP

#include <thread>
#include <atomic>

#include "defines.hpp"

// This class is intrusive.
template<typename T>
class blocking_cached_stack {

    // singly linked list, serves as cache
    T* m_head;
    char m_pad1[CACHE_LINE_SIZE - sizeof(T*)];

    // modified by consumers
    std::atomic<T*> m_stack;
    char m_pad2[CACHE_LINE_SIZE - sizeof(std::atomic<T*>)];

    // locked on enqueue/dequeue operations to/from an empty list
    std::mutex m_mtx;
    std::condition_variable m_cv;

    typedef std::unique_lock<std::mutex> lock_type;

    // read all elements of m_stack, convert them to FIFO order and store
    // them in m_head
    // precondition: m_head == nullptr
    bool consume_stack() {
        T* e = m_stack.load();
        while (e) {
            if (m_stack.compare_exchange_weak(e, 0)) {
                // m_stack is now empty (m_stack == nullptr)
                while (e) {
                    T* next = e->next;
                    // enqueue to m_head
                    e->next = m_head;
                    m_head = e;
                    // next iteration
                    e = next;
                }
                return true;
            }
        }
        // nothing to consume
        return false;
    }

    void wait_for_data() {
        if (!m_head && !(m_stack.load())) {
            lock_type lock(m_mtx);
            while (!(m_stack.load())) m_cv.wait(lock);
        }
    }

 public:

    blocking_cached_stack() : m_head(0) {
        m_stack = 0;
    }

    ~blocking_cached_stack() {
        do {
            while (m_head) {
                T* next = m_head->next;
                delete m_head;
                m_head = next;
            }
        }
        // repeat if m_stack is not empty
        while (consume_stack());
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

    T* try_pop() {
        if (m_head || consume_stack()) {
            T* result = m_head;
            m_head = m_head->next;
            return result;
        }
        return 0;
    }

    T* pop() {
        wait_for_data();
        return try_pop();
    }

};

#endif // BLOCKING_CACHED_STACK_HPP
