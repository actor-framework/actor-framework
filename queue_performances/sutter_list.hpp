#ifndef SUTTER_LIST_HPP
#define SUTTER_LIST_HPP

#include <atomic>
#include <boost/thread.hpp>

#include "defines.hpp"

// This implementation is a single-consumer version
// of an example implementation of Herb Sutter:
// http://drdobbs.com/cpp/211601363.

// T is any type
template<typename T>
class sutter_list {

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

 public:

	sutter_list() {
		m_first = m_last = new node;
		m_producer_lock = false;
	}

	~sutter_list() {
		while (m_first) {
			node* tmp = m_first;
			m_first = tmp->next;
			delete tmp;
		}
	}

	// takes ownership of what
	void push(T* what) {
		node* tmp = new node(what);
		// acquire exclusivity
		while (m_producer_lock.exchange(true)) {
			std::this_thread::yield();
		}
		// publish & swing last forward
		m_last->next = tmp;
		m_last = tmp;
		// release exclusivity
		m_producer_lock = false;
	}

	// returns nullptr on failure
	T* try_pop() {
		// no critical section; only one consumer allowed
		node* first = m_first;
		node* next = m_first->next;
		if (next) {
			// queue is not empty
			T* result = next->value; // take it out
			next->value = 0;         // of the node
			// swing first forward
			m_first = next;
			// delete old dummy
			delete first;
			// done
			return result;
		}
		// queue was empty
		return 0;
	}

	// polls the queue until an element was dequeued
	T* pop() {
		T* result = try_pop();
		while (!result) {
			std::this_thread::yield();
			result = try_pop();
		}
		return result;
	}

};

#endif // SUTTER_LIST_HPP
