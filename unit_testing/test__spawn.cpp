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
#include "cppa/fsm_actor.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/event_based_actor.hpp"
#include "cppa/stacked_event_based_actor.hpp"

using std::cerr;
using std::cout;
using std::endl;

using namespace cppa;

// GCC 4.7 supports non-static member initialization
#if (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 7)

struct event_testee : public fsm_actor<event_testee>
{

    invoke_rules wait4string =
    (
        on<std::string>() >> [this]()
        {
            become(&init_state);
        },
        on<atom("GetState")>() >> [=]()
        {
            reply("wait4string");
        }
    );

    invoke_rules wait4float =
    (
        on<float>() >> [=]()
        {
            become(&wait4string);
        },
        on<atom("GetState")>() >> [=]()
        {
            reply("wait4float");
        }
    );

    invoke_rules init_state =
    (
        on<int>() >> [=]()
        {
            become(&wait4float);
        },
        on<atom("GetState")>() >> [=]()
        {
            reply("init_state");
        }
    );

};

#else

class event_testee : public fsm_actor<event_testee>
{

    friend class fsm_actor<event_testee>;

    invoke_rules wait4string;
    invoke_rules wait4float;
    invoke_rules init_state;

 public:

    event_testee()
    {
        wait4string =
        (
            on<std::string>() >> [=]()
            {
                become(&init_state);
            },
            on<atom("GetState")>() >> [=]()
            {
                reply("wait4string");
            }
        );

        wait4float =
        (
            on<float>() >> [=]()
            {
                //become(&wait4string);
                become
                (
                    on<std::string>() >> [=]()
                    {
                        become(&init_state);
                    },
                    on<atom("GetState")>() >> []()
                    {
                        reply("wait4string");
                    }
                );
            },
            on<atom("GetState")>() >> [=]()
            {
                reply("wait4float");
            }
        );

        init_state =
        (
            on<int>() >> [=]()
            {
                become(&wait4float);
            },
            on<atom("GetState")>() >> [=]()
            {
                reply("init_state");
            }
        );
    }

};

#endif

// quits after 5 timeouts
abstract_event_based_actor* event_testee2()
{
    struct impl : fsm_actor<impl>
    {
        int num_timeouts;
        timed_invoke_rules init_state;
        impl() : num_timeouts(0)
        {
            init_state =
            (
                others() >> []()
                {
                    cout << "event testee2: "
                         << to_string(last_received())
                         << endl;
                },
                after(std::chrono::milliseconds(50)) >> [=]()
                {
                    if (++num_timeouts >= 5)
                    {
                        quit(exit_reason::normal);
                    }
                }
            );
        }
    };
    return new impl;
}

class testee_actor : public scheduled_actor
{

    void wait4string()
    {
        bool string_received = false;
        receive_while([&]() { return !string_received; })
        (
            on<std::string>() >> [&]()
            {
                string_received = true;
            },
            on<atom("GetState")>() >> [&]()
            {
                reply("wait4string");
            }
        );
    }

    void wait4float()
    {
        bool float_received = false;
        receive_while([&]() { return !float_received; })
        (
            on<float>() >> [&]()
            {
                float_received = true;
                wait4string();
            },
            on<atom("GetState")>() >> [&]()
            {
                reply("wait4float");
            }
        );
    }

 public:

    void act()
    {
        receive_loop
        (
            on<int>() >> [&]()
            {
                wait4float();
            },
            on<atom("GetState")>() >> [&]()
            {
                reply("init_state");
            }
        );
    }

};

// receives one timeout and quits
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
    future_send(self, std::chrono::milliseconds(50), atom("Poll"));
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
std::string behavior_test()
{
    std::string result;
    std::string testee_name = detail::to_uniform_name(typeid(Testee));
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
    send(et, atom("GetState"));
    receive
    (
        on<std::string>() >> [&](const std::string& str)
        {
            result = str;
        },
        after(std::chrono::seconds(2)) >> [&]()
        {
            throw std::runtime_error(testee_name + " does not reply");
        }
    );
    send(et, atom(":Exit"), exit_reason::user_defined);
    await_all_others_done();
    return result;
}

size_t test__spawn()
{
    CPPA_TEST(test__spawn);

    spawn(testee1);
    spawn(event_testee2());

    await_all_others_done();

    CPPA_CHECK_EQUAL(behavior_test<testee_actor>(), "init_state");
    CPPA_CHECK_EQUAL(behavior_test<event_testee>(), "init_state");

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
    future_send(self, std::chrono::seconds(1), atom("FooBar"));
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
    spawn(testee3, self);
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
