#ifndef INTRUSIVE_SUTTER_LIST_HPP
#define INTRUSIVE_SUTTER_LIST_HPP

#include <atomic>
#include <boost/thread.hpp>

#include "defines.hpp"

// This implementation is a single-consumer version
// of an example implementation of Herb Sutter:
// http://drdobbs.com/cpp/211601363.

// T is any type
template<typename T>
class intrusive_sutter_list {

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

	// for one producers at a time
	node* m_last;
	char m_pad2[CACHE_LINE_SIZE - sizeof(node*)];

	// shared among producers
	std::atomic<bool> m_producer_lock;

 public:

	intrusive_sutter_list() {
		m_first = m_last = new node;
		m_producer_lock = false;
	}

	~intrusive_sutter_list() {
		while (m_first) {
			node* tmp = m_first;
			m_first = tmp->next;
			delete tmp;
		}
	}

	// takes ownership of what
	void push(node* tmp) {
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
	bool try_pop(T& result) {
		// no critical section; only one consumer allowed
		node* first = m_first;
		node* next = m_first->next;
		if (next) {
			// queue is not empty
			result = next->value; // take it out of the node
			// swing first forward
			m_first = next;
			// delete old dummy
			delete first;
			// done
			return true;
		}
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

#endif // INTRUSIVE_SUTTER_LIST_HPP
