#ifndef LOCKFREE_LIST_HPP
#define LOCKFREE_LIST_HPP

#include <atomic>
#include <boost/thread.hpp>

#include "defines.hpp"

// This implementation is a single-consumer version
// of an example implementation of Herb Sutter:
// http://drdobbs.com/cpp/211601363.

// T is any type
template<typename T>
class lockfree_list {

 public:

	struct node {
		node(T val = T()) : value(val), next(0) { }
		T value;
		std::atomic<node*> next;
		char pad[CACHE_LINE_SIZE - sizeof(T)- sizeof(std::atomic<node*>)];
	};

 private:

	// one consumer at a time
	node* m_first;
	char m_pad1[CACHE_LINE_SIZE - sizeof(node*)];

	// shared among producers
	std::atomic<node*> m_last;

 public:

	lockfree_list() {
		m_first = m_last = new node;
	}

	~lockfree_list() {
		while (m_first) {
			node* tmp = m_first;
			m_first = tmp->next;
			delete tmp;
		}
	}

	// takes ownership of what
	void push(node* tmp) {
		// m_last becomes our predecessor
		node* predecessor = m_last.load();
		// swing last forward
		while (!m_last.compare_exchange_weak(predecessor, tmp)) { }
		// link pieces together
		predecessor->next = tmp;
	}

	// returns nullptr on failure
	bool try_pop(T& result) {
		// no critical section; only one consumer allowed
		node* first = m_first;
		node* next = m_first->next;
		if (next) {
			// queue is not empty
			result = next->value; // take it out
			next->value = 0;         // of the node
			// swing first forward
			m_first = next;
			// delete old dummy
			delete first;
			// done
			return true;
		}
		// queue was empty
		return false;
	}

	// polls the queue until an element was dequeued
	T pop() {
		T result;
		while (!try_pop(result)) {
			std::this_thread::yield();
		}
		return result;
	}

};

#endif // LOCKFREE_LIST_HPP
