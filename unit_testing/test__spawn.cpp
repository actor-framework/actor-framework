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
#include "cppa/event_based_actor.hpp"

using std::cerr;
using std::cout;
using std::endl;

using namespace cppa;

class event_testee : public event_based_actor
{

    invoke_rules wait4string()
    {
        return on<std::string>() >> [=](const std::string& value)
        {
            cout << "event_testee[string]: " << value << endl;
            // switch back to wait4int
            unbecome();
            unbecome();
        };
    }

    invoke_rules wait4float()
    {
        return on<float>() >> [=](float value)
        {
            cout << "event_testee[float]: " << value << endl;
            become(wait4string());
        };
    }

    invoke_rules wait4int()
    {
        return on<int>() >> [=](int value)
        {
            cout << "event_testee[int]: " << value << endl;
            become(wait4float());
        };
    }

 public:

    void on_exit()
    {
        cout << "event_testee::on_exit()" << endl;
    }

    void init()
    {
        cout << "event_testee::init()" << endl;
        become(wait4int());
    }

};

event_based_actor* event_testee2()
{
    struct impl : event_based_actor
    {
        int num_timeouts;
        impl() : num_timeouts(0) { }
        void init()
        {
            become
            (
                others() >> []()
                {
                    cout << "event testee2: "
                         << to_string(last_received())
                         << endl;
                },
                after(std::chrono::milliseconds(50)) >> [this]()
                {
                    cout << "testee2 received timeout nr. "
                         << (num_timeouts + 1) << endl;
                    if (++num_timeouts >= 5) unbecome();
                }
            );
        }
    };
    return new impl;
}

class testee_behavior : public actor_behavior
{

 public:

    void act()
    {
        receive_loop
        (
            on<int>() >> [&](int i)
            {
                cout << "testee_behavior[int]: " << i << endl;
                receive
                (
                    on<float>() >> [&](float f)
                    {
                        cout << "testee_behavior[float]: " << f << endl;
                        receive
                        (
                            on<std::string>() >> [&](const std::string& str)
                            {
                                cout << "testee_behavior[string]: "
                                     << str
                                     << endl;
                            }
                        );
                    }
                );
            }
        );
    }

    void on_exit()
    {
        cout << "testee_behavior::on_exit()" << endl;
    }

};

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

template<class Testee>
void behavior_test()
{
    std::string testee_name = detail::to_uniform_name(typeid(Testee));
    cout << "behavior_test<" << testee_name << ">()" << endl;
    auto et = spawn(new Testee);
    send(et, 1);
    send(et, 2);
    send(et, 3);
    send(et, .1f);
    send(et, "hello " + testee_name);
    send(et, .2f);
    send(et, .3f);
    send(et, "hello again " + testee_name);
    send(et, "goodbye " + testee_name);
    send(et, atom(":Exit"), exit_reason::user_defined);
    await_all_others_done();
    cout << endl;
}

size_t test__spawn()
{
    CPPA_TEST(test__spawn);

    //spawn(testee1);
    //spawn(new testee_behavior);

    //await_all_others_done();

    //spawn(event_testee2());

    //auto et = spawn(new event_testee);

    behavior_test<testee_behavior>();
    behavior_test<event_testee>();

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
