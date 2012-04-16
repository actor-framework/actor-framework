#include <list>
#include <atomic>
#include <iostream>
#include "test.hpp"

#include <boost/ref.hpp>
#include <boost/thread.hpp>
#include <boost/progress.hpp>

#include "cppa/intrusive/single_reader_queue.hpp"

//#define DEBUG_RESULTS

// "config"

/*

namespace {

const size_t slave_messages = 1000000;
const size_t trials = 10;

} // namespace <anonymous>



using cppa::intrusive::single_reader_queue;

using std::cout;
using std::cerr;
using std::endl;

struct queue_element
{
    queue_element* next;
    size_t value;
    queue_element(size_t val = 0) : next(0), value(val) { }
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

    typedef std::unique_ptr<element_type> unique_pointer;

    unique_pointer pop()
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

    void push_back(element_type* new_element)
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

//const size_t num_slaves = 1000;
//const size_t num_slave_msgs = 90000;

// 900 000
//const size_t num_msgs = (num_slaves) * (num_slave_msgs);

// uint32::max   = 4 294 967 296
// (n (n+1)) / 2 = 4 050 045 000
//const size_t calc_result = ((num_msgs)*(num_msgs + 1)) / 2;

} // namespace <anonymous>

template<typename Queue>
void slave(Queue& q, size_t from, size_t to)
{
    for (size_t x = from; x < to; ++x)
    {
        q.push_back(new queue_element(x));
    }
}

template<typename Queue, size_t num_slaves, size_t num_slave_msgs>
void master()
{

    static const size_t num_msgs = (num_slaves) * (num_slave_msgs);

    static const size_t calc_result = ((num_msgs)*(num_msgs + 1)) / 2;


    //cout << num_slaves << " workers; running test";
    //cout.flush();

    double elapsed[trials];

    for (size_t i = 0; i < trials; ++i)
    {

        //cout << " ... " << (i + 1);
        //cout.flush();

        Queue q;

        boost::timer t0;
        for (size_t j = 0; j < num_slaves; ++j)
        {
            size_t from = (j * num_slave_msgs) + 1;
            size_t to = from + num_slave_msgs;
            boost::thread(slave<Queue>, boost::ref(q), from, to).detach();
        }
        size_t result = 0;
#       ifdef DEBUG_RESULTS
        size_t min_val = calc_result;
        size_t max_val = 0;
#       endif
        for (size_t j = 0; j < num_msgs; ++j)
        {
            std::unique_ptr<queue_element> e = q.pop();
            result += e->value;
#           ifdef DEBUG_RESULTS
            min_val = std::min(min_val, e->value);
            max_val = std::max(max_val, e->value);
#           endif
        }
        if (result != calc_result)
        {
            cerr << "ERROR: result = " << result
                 << " (should be: " << calc_result << ")"
#               ifdef DEBUG_RESULTS
                 << endl << "min: " << min_val
                 << endl << "max: " << max_val
#               endif
                 << endl;
        }
        elapsed[i] = t0.elapsed();
        //cout << t0.elapsed() << " " << num_slaves << endl;
    }
    //cout << endl;
    double sum = 0;
    //cout << "runtimes = { ";
    for (size_t i = 0; i < trials; ++i)
    {
         //cout << (i == 0 ? "" : ", ") << elapsed[i];
         sum += elapsed[i];
    }
    //cout << " }" << endl;
    //cout << "AVG = " << (sum / trials) << endl;
    cout << (sum / trials) << " " << num_slaves << endl;
}

template<size_t Pos, size_t Max, size_t Step,
         template<size_t> class Stmt>
struct static_for
{
    template<typename... Args>
    static inline void _(const Args&... args)
    {
        Stmt<Pos>::_(args...);
        static_for<Pos + Step, Max, Step, Stmt>::_(args...);
    }
};

template<size_t Max, size_t Step,
         template<size_t> class Stmt>
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

template<size_t NumThreads>
struct test_step
{
    template<typename QueueToken>
    static inline void _(QueueToken)
    {
        boost::thread t0(master<typename QueueToken::type, NumThreads, slave_messages>);
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
    cout << "Format: "
            "(average value of 10 runs) "
//            "(standard deviation) "
            "(number of worker threads)"
         << endl;
    cout << "locked_queue:" << endl;
    test_q_impl<locked_queue<queue_element>>();
    cout << endl;
    cout << "single_reader_queue:" << endl;
    test_q_impl<single_reader_queue<queue_element>>();
}
*/

void test__queue_performance()
{

}
