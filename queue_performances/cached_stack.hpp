#ifndef CACHED_STACK_HPP
#define CACHED_STACK_HPP

#include <atomic>
#include <boost/thread.hpp>

#include "defines.hpp"

// This class is intrusive.
template<typename T>
class cached_stack {

	// singly linked list, serves as cache
	T* m_head;
	char m_pad1[CACHE_LINE_SIZE - sizeof(T*)];

	// modified by consumers
	std::atomic<T*> m_stack;

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

 public:

	cached_stack() : m_head(0) {
		m_stack = 0;
	}

	~cached_stack() {
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
			// compare_exchange_weak stores the
			// new value to e if the operation fails
			if (m_stack.compare_exchange_weak(e, what)) return;
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
		T* result = try_pop();
		while (!result) {
			std::this_thread::yield();
			result = try_pop();
		}
		return result;
	}

};

#endif // CACHED_STACK_HPP
