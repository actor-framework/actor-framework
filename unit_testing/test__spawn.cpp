#define CPPA_VERBOSE_CHECK

#include <stack>
#include <chrono>
#include <iostream>
#include <functional>

#include "test.hpp"
#include "ping_pong.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exit_reason.hpp"

using std::cerr;
using std::cout;
using std::endl;

using namespace cppa;

namespace { const any_tuple* s_lm = nullptr; }

class event_actor
{

    std::stack<invoke_rules> m_behavior;
    invoke_rules m_next_behavior;

    invoke_rules* m_behavior_ptr;

 public:

    event_actor(invoke_rules&& behavior)
    {
        m_behavior.push(std::move(behavior));
        m_behavior_ptr = &(m_behavior.top());
    }

    void become(invoke_rules&& behavior)
    {
        m_behavior.push(std::move(behavior));
        m_behavior_ptr = &(m_behavior.top());
    }

    void set_next_behavior(invoke_rules&& behavior)
    {
        m_next_behavior = std::move(behavior);
        m_behavior_ptr = &m_next_behavior;
    }

    void unbecome()
    {
        if (!m_behavior.empty())
        {
            if (m_behavior_ptr == &(m_behavior.top()))
            {
                m_behavior.pop();
                m_behavior_ptr = m_behavior.empty() ? nullptr
                                                    : &(m_behavior.top());
            }
            else
            {
                m_behavior.pop();
            }
        }
    }

    void operator()(const any_tuple& msg)
    {
        if (m_behavior_ptr != nullptr)
        {
            s_lm = &msg;
            auto ptr = m_behavior_ptr;
            (*ptr)(msg);
            // execute m_next_behavior at most once
            if (ptr == m_behavior_ptr && ptr == &m_next_behavior)
            {
                m_behavior_ptr = m_behavior.empty() ? nullptr
                                                    : &m_behavior.top();
            }
        }
    }

};

namespace { event_actor* s_event_actor_self = nullptr; }

void set_next_behavior(invoke_rules&& behavior)
{
    s_event_actor_self->set_next_behavior(std::move(behavior));
}

void become(invoke_rules&& behavior)
{
    s_event_actor_self->become(std::move(behavior));
}

void unbecome()
{
    s_event_actor_self->unbecome();
}

event_actor* event_testee()
{
    return new event_actor
    {(
        on<int>() >> [](int i)
        {
            // do NOT call receive() here!
            // this would hijack the worker thread
            set_next_behavior
            ((
                on<int>() >> [=](int i2)
                {
                    cout << "event testee: (" << i << ", " << i2 << ")" << endl;
                },
                on<float>() >> [=](float f)
                {
                    cout << "event testee: (" << i << ", " << f << ")" << endl;
                    become
                    ((
                        on<float>() >> []()
                        {
                            unbecome();
                        },
                        others() >> []()
                        {
                            cout << "event testee[line " << __LINE__ << "]: "
                                 << to_string(*s_lm)
                                 << endl;
                        }
                    ));
                }
            ));
        },
        others() >> []()
        {
            cout << "event testee[line " << __LINE__ << "]: "
                 << to_string(*s_lm)
                 << endl;
        }
    )};
}

void testee1()
{
    receive_loop
    (
        after(std::chrono::milliseconds(10)) >> []()
        {
            quit(exit_reason::user_defined);
        }
    );
}

void testee2(actor_ptr other)
{
    link(other);
    send(other, std::uint32_t(1));
    receive_loop
    (
        on<std::uint32_t>() >> [](std::uint32_t sleep_time)
        {
            // "sleep" for sleep_time milliseconds
            receive(after(std::chrono::milliseconds(sleep_time)) >> []() {});
            //reply(sleep_time * 2);
        }
    );
}

void testee3(actor_ptr parent)
{
    // test a future_send / delayed_reply based loop
    future_send(self(), std::chrono::milliseconds(50), atom("Poll"));
    int polls = 0;
    receive_while([&polls]() { return ++polls <= 5; })
    (
        on(atom("Poll")) >> [&]()
        {
            if (polls < 5)
            {
                //delayed_reply(std::chrono::milliseconds(50), atom("Poll"));
            }
            send(parent, atom("Push"), polls);
        }
    );
}

size_t test__spawn()
{
    CPPA_TEST(test__spawn);

    spawn(testee1);
    await_all_others_done();

    auto et = event_testee();
    s_event_actor_self = et;
    (*et)(make_tuple(42));
    (*et)(make_tuple(24));
    (*et)(make_tuple(42));
    (*et)(make_tuple(.24f));
    (*et)(make_tuple("hello event actor"));
    (*et)(make_tuple(42));
    (*et)(make_tuple(.24f));
    (*et)(make_tuple("hello event actor"));
    delete et;

    return CPPA_TEST_RESULT;






    auto report_unexpected = [&]()
    {
        cerr << "unexpected message: " << to_string(last_received()) << endl;
        CPPA_CHECK(false);
    };
    trap_exit(true);
    auto pong_actor = spawn(pong, spawn(ping));
    monitor(pong_actor);
    link(pong_actor);
//    monitor(spawn(testee2, spawn(testee1)));
    int i = 0;
    int flags = 0;
    future_send(self(), std::chrono::seconds(1), atom("FooBar"));
    // wait for :Down and :Exit messages of pong
    receive_while([&i]() { return ++i <= 3 /*4*/; })
    (
        on<atom(":Exit"), std::uint32_t>() >> [&](std::uint32_t reason)
        {
            CPPA_CHECK_EQUAL(reason, exit_reason::user_defined);
            //CPPA_CHECK_EQUAL(who, pong_actor);
            flags |= 0x01;
        },
        on<atom(":Down"), actor_ptr, std::uint32_t>() >> [&](const actor_ptr& who,
                                                             std::uint32_t reason)
        {
            CPPA_CHECK_EQUAL(reason, exit_reason::user_defined);
            if (who == pong_actor)
            {
                flags |= 0x02;
            }
        },
        on<atom("FooBar")>() >> [&]()
        {
            flags |= 0x04;
        },
        others() >> [&]()
        {
            report_unexpected();
            CPPA_CHECK(false);
        },
        after(std::chrono::seconds(5)) >> [&]()
        {
            cout << "!!! TIMEOUT !!!" << endl;
            CPPA_CHECK(false);
        }

    );
    // wait for termination of all spawned actors
    await_all_others_done();
    CPPA_CHECK_EQUAL(flags, 0x07);
    // verify pong messages
    CPPA_CHECK_EQUAL(pongs(), 5);
    /*
    spawn(testee3, self());
    i = 0;
    // testee3 sends 5 { "Push", int } messages in a 50 milliseconds interval;
    // allow for a maximum error of 5ms
    receive_while([&i]() { return ++i <= 5; })
    (
        on<atom("Push"), int>() >> [&](int val)
        {
            CPPA_CHECK_EQUAL(i, val);
            //cout << "{ Push, " << val << " } ..." << endl;
        },
        after(std::chrono::milliseconds(55)) >> [&]()
        {
            cout << "Push " << i
                 << " was delayed more than 55 milliseconds" << endl;
            CPPA_CHECK(false);
        }
    );
    await_all_others_done();
    */
    return CPPA_TEST_RESULT;
}
