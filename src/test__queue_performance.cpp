#include <list>
#include <atomic>
#include <iostream>
#include "cppa/test.hpp"

#include <boost/ref.hpp>
#include <boost/thread.hpp>
#include <boost/progress.hpp>

using std::cout;
using std::cerr;
using std::endl;

struct queue_element
{
	queue_element* next;
	std::size_t value;
	queue_element(std::size_t val) : next(0), value(val) { }
};

/**
 * @brief An intrusive, thread safe queue implementation.
 */
template<typename T>
class single_reader_queue
{

	typedef boost::unique_lock<boost::mutex> lock_type;

 public:

	typedef T element_type;

	element_type* pop()
	{
		wait_for_data();
		element_type* result = take_head();
		return result;
	}

	element_type* try_pop()
	{
		return take_head();
	}

/*
	element_type* front()
	{
		return (m_head || fetch_new_data()) ? m_head : 0;
	}
*/

	void push(element_type* new_element)
	{
		element_type* e = m_tail.load();
		for (;;)
		{
//			element_type* e = const_cast<element_type*>(m_tail);
			new_element->next = e;
			if (!e)
			{
				lock_type guard(m_mtx);
//				if (atomic_cas(&m_tail, (element_type*) 0, new_element))
				if (m_tail.compare_exchange_weak(e, new_element))
				{
					m_cv.notify_one();
					return;
				}
			}
			else
			{
//				if (atomic_cas(&m_tail, e, new_element))
				if (m_tail.compare_exchange_weak(e, new_element))
				{
					return;
				}
			}
		}
	}

	single_reader_queue() : m_tail(0), m_head(0) { }

 private:

	// exposed to "outside" access

	atomic<element_type*> m_tail;
//	volatile element_type* m_tail;

	// accessed only by the owner
	element_type* m_head;

	// locked on enqueue/dequeue operations to/from an empty list
	boost::mutex m_mtx;
	boost::condition_variable m_cv;

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
//			if (atomic_cas(&m_tail, e, (element_type*) 0))
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
//			e = const_cast<element_type*>(m_tail);
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

template<typename T>
class locked_queue
{

	typedef boost::unique_lock<boost::mutex> lock_type;

 public:

	typedef T element_type;

	element_type* pop()
	{
		lock_type guard(m_mtx);
		element_type* result;
		while (m_impl.empty())
		{
			m_cv.wait(guard);
		}
		result = m_impl.front();
		m_impl.pop_front();
		return result;
	}

	void push(element_type* new_element)
	{
		lock_type guard(m_mtx);
		if (m_impl.empty())
		{
			m_cv.notify_one();
		}
		m_impl.push_back(new_element);
	}

 private:

	boost::mutex m_mtx;
	boost::condition_variable m_cv;
	std::list<element_type*> m_impl;

};

namespace {

//const std::size_t num_slaves = 1000;
//const std::size_t num_slave_msgs = 90000;

// 900 000
//const std::size_t num_msgs = (num_slaves) * (num_slave_msgs);

// uint32::max   = 4 294 967 296
// (n (n+1)) / 2 = 4 050 045 000
//const std::size_t calc_result = ((num_msgs)*(num_msgs + 1)) / 2;

} // namespace <anonymous>

template<typename Queue>
void slave(Queue& q, std::size_t from, std::size_t to)
{
	for (std::size_t x = from; x < to; ++x)
	{
		q.push(new queue_element(x));
	}
}

template<typename Queue, std::size_t num_slaves, std::size_t num_slave_msgs>
void master(Queue& q)
{

	static const std::size_t num_msgs = (num_slaves) * (num_slave_msgs);

	static const std::size_t calc_result = ((num_msgs)*(num_msgs + 1)) / 2;

	boost::timer t0;
	for (std::size_t i = 0; i < num_slaves; ++i)
	{
		std::size_t from = (i * num_slave_msgs) + 1;
		std::size_t to = from + num_slave_msgs;
		boost::thread(slave<Queue>, boost::ref(q), from, to).detach();
	}
	std::size_t result = 0;
	std::size_t min_val = calc_result;
	std::size_t max_val = 0;
	for (std::size_t i = 0; i < num_msgs; ++i)
	{
		queue_element* e = q.pop();
		result += e->value;
		min_val = std::min(min_val, e->value);
		max_val = std::max(max_val, e->value);
		delete e;
	}
	if (result != calc_result)
	{
		cerr << "ERROR: result = " << result
			 << " (should be: " << calc_result << ")" << endl
			 << "min: " << min_val << endl
			 << "max: " << max_val << endl;
	}
	cout << t0.elapsed() << " " << num_msgs << endl;
}

template<typename Queue>
void test_q_impl()
{
	typedef Queue queue_type;
	{
		queue_type q0;
		boost::thread t0(master<queue_type, 10, 10000>, boost::ref(q0));
		t0.join();
	}
	{
		queue_type q0;
		boost::thread t0(master<queue_type, 100, 10000>, boost::ref(q0));
		t0.join();
	}
	{
		queue_type q0;
		boost::thread t0(master<queue_type, 1000, 10000>, boost::ref(q0));
		t0.join();
	}
	{
		queue_type q0;
		boost::thread t0(master<queue_type, 10000, 10000>, boost::ref(q0));
		t0.join();
	}
}

void test__queue_performance()
{
	cout << "single_reader_queue:" << endl;
	test_q_impl<single_reader_queue<queue_element>>();
	cout << endl << "locked_queue:" << endl;
	test_q_impl<locked_queue<queue_element>>();
}
