#include <list>
#include <atomic>
#include <iostream>
#include "test.hpp"

#include <boost/ref.hpp>
#include <boost/thread.hpp>
#include <boost/progress.hpp>

#include "cppa/util/single_reader_queue.hpp"

using cppa::util::single_reader_queue;

using std::cout;
using std::cerr;
using std::endl;

struct queue_element
{
	queue_element* next;
	std::size_t value;
	queue_element(std::size_t val) : next(0), value(val) { }
};

template<typename T>
class singly_linked_list
{

	typedef T element_type;

	element_type* m_head;
	element_type* m_tail;

 public:

	singly_linked_list() : m_head(0), m_tail(0) { }

	singly_linked_list& operator=(singly_linked_list&& other)
	{
		m_head = other.m_head;
		m_tail = other.m_tail;
		other.m_head = 0;
		other.m_tail = 0;
		return *this;
	}

	inline bool empty() const { return m_head == 0; }

	void push_back(element_type* e)
	{
		if (!m_head)
		{
			m_head = m_tail = e;
		}
		else
		{
			m_tail->next = e;
			m_tail = e;
		}
	}

	element_type* pop_front()
	{
		element_type* result = m_head;
		if (result)
		{
			m_head = result->next;
			if (!m_head)
			{
				m_tail = 0;
			}
		}
		return result;
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
		if (!m_priv.empty())
		{
			return m_priv.pop_front();
		}
		else
		{
			// lifetime scope of guard
			{
				lock_type guard(m_mtx);
				while (m_pub.empty())
				{
					m_cv.wait(guard);
				}
				m_priv = std::move(m_pub);
			}
			// tail recursion
			return pop();
		}
	}

	void push(element_type* new_element)
	{
		lock_type guard(m_mtx);
		if (m_pub.empty())
		{
			m_cv.notify_one();
		}
		m_pub.push_back(new_element);
	}

 private:

	boost::mutex m_mtx;
	boost::condition_variable m_cv;
	singly_linked_list<element_type> m_pub;
	singly_linked_list<element_type> m_priv;

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
		q.push_back(new queue_element(x));
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
	cout << t0.elapsed() << " " << num_slaves << endl;
}

namespace { const std::size_t slave_messages = 1000000; }

template<std::size_t Pos, std::size_t Max, std::size_t Step,
		 template<std::size_t> class Stmt>
struct static_for
{
	template<typename... Args>
	static inline void _(const Args&... args)
	{
		Stmt<Pos>::_(args...);
		static_for<Pos + Step, Max, Step, Stmt>::_(args...);
	}
};

template<std::size_t Max, std::size_t Step,
		 template<std::size_t> class Stmt>
struct static_for<Max, Max, Step, Stmt>
{
	template<typename... Args>
	static inline void _(const Args&... args)
	{
		Stmt<Max>::_(args...);
	}
};

template<typename What>
struct type_token
{
	typedef What type;
};

template<std::size_t NumThreads>
struct test_step
{
	template<typename QueueToken>
	static inline void _(QueueToken)
	{
		typename QueueToken::type q;
		boost::thread t0(master<typename QueueToken::type, NumThreads, slave_messages>, boost::ref(q));
		t0.join();
	}
};

template<typename Queue>
void test_q_impl()
{
	typedef type_token<Queue> queue_token;
	static_for<10, 50, 5, test_step>::_(queue_token());
}

void test__queue_performance()
{
	cout << "locked_queue:" << endl;
//	test_q_impl<locked_queue<queue_element>>();
	cout << endl;
	cout << "single_reader_queue:" << endl;
	test_q_impl<single_reader_queue<queue_element>>();
}
