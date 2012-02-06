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

    behavior wait4string =
    (
        on<std::string>() >> [=]()
        {
            become(&wait4int);
        },
        on<atom("get_state")>() >> [=]()
        {
            reply("wait4string");
        }
    );

    behavior wait4float =
    (
        on<float>() >> [=]()
        {
            become(&wait4string);
        },
        on<atom("get_state")>() >> [=]()
        {
            reply("wait4float");
        }
    );

    behavior wait4int =
    (
        on<int>() >> [=]()
        {
            become(&wait4float);
        },
        on<atom("get_state")>() >> [=]()
        {
            reply("wait4int");
        }
    );

    behavior& init_state = wait4int;

};

#else

class event_testee : public fsm_actor<event_testee>
{

    friend class fsm_actor<event_testee>;

    behavior wait4string;
    behavior wait4float;
    behavior wait4int;

    behavior& init_state;

 public:

    event_testee() : init_state(wait4int)
    {
        wait4string =
        (
            on<std::string>() >> [=]()
            {
                become(&wait4int);
            },
            on<atom("get_state")>() >> [=]()
            {
                reply("wait4string");
            }
        );

        wait4float =
        (
            on<float>() >> [=]()
            {
                become(&wait4string);
            },
            on<atom("get_state")>() >> [=]()
            {
                reply("wait4float");
            }
        );

        wait4int =
        (
            on<int>() >> [=]()
            {
                become(&wait4float);
            },
            on<atom("get_state")>() >> [=]()
            {
                reply("wait4int");
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
        behavior wait4timeout(int remaining)
        {
            return
            (
                after(std::chrono::milliseconds(50)) >> [=]()
                {
                    if (remaining == 1) become_void();
                    else become(wait4timeout(remaining-1));
                }
            );
        }

        behavior init_state;

        impl() : init_state(wait4timeout(5)) { }
    };
    return new impl;
}

struct chopstick : public fsm_actor<chopstick>
{

    behavior taken_by(actor_ptr hakker)
    {
        return
        (
            on<atom("take")>() >> [=]()
            {
                reply(atom("busy"));
            },
            on(atom("put"), hakker) >> [=]()
            {
                become(&init_state);
            },
            on(atom("break")) >> [=]()
            {
                become_void();
            }
        );
    }

    behavior init_state;

    chopstick()
    {
        init_state =
        (
            on(atom("take"), arg_match) >> [=](actor_ptr hakker)
            {
                become(taken_by(hakker));
                reply(atom("taken"));
            },
            on(atom("break")) >> [=]()
            {
                become_void();
            },
            others() >> [=]()
            {
            }
        );
    }

};

class testee_actor : public scheduled_actor
{

    void wait4string()
    {
        bool string_received = false;
        do_receive
        (
            on<std::string>() >> [&]()
            {
                string_received = true;
            },
            on<atom("get_state")>() >> [&]()
            {
                reply("wait4string");
            }
        )
        .until([&]() { return string_received; });
    }

    void wait4float()
    {
        bool float_received = false;
        do_receive
        (
            on<float>() >> [&]()
            {
                float_received = true;
                wait4string();
            },
            on<atom("get_state")>() >> [&]()
            {
                reply("wait4float");
            }
        )
        .until([&]() { return float_received; });
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
            on<atom("get_state")>() >> [&]()
            {
                reply("wait4int");
            }
        );
    }

};

// receives one timeout and quits
void testee1()
{
    receive
    (
        after(std::chrono::milliseconds(10)) >> []() { }
    );
}

void testee2(actor_ptr other)
{
    self->link_to(other);
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
    send(et, atom("get_state"));
    receive
    (
        on_arg_match >> [&](const std::string& str)
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

    auto cstk = spawn(new chopstick);
    send(cstk, atom("take"), self);
    receive
    (
        on(atom("taken")) >> [&]()
        {
            send(cstk, atom("put"), self);
            send(cstk, atom("break"));
        }
    );

    await_all_others_done();

    CPPA_CHECK_EQUAL(behavior_test<testee_actor>(), "wait4int");
    CPPA_CHECK_EQUAL(behavior_test<event_testee>(), "wait4int");

    // create 20,000 actors linked to one single actor
    // and kill them all through killing the link
    auto my_link = spawn(new event_testee);
    for (int i = 0; i < 20000; ++i)
    {
        link(my_link, spawn(new event_testee));
    }
    send(my_link, atom(":Exit"), exit_reason::user_defined);
    await_all_others_done();

    auto report_unexpected = [&]()
    {
        cerr << "unexpected message: "
             << to_string(self->last_dequeued()) << endl;
        CPPA_CHECK(false);
    };
    self->trap_exit(true);
    auto pong_actor = spawn(pong, spawn(ping));
    monitor(pong_actor);
    self->link_to(pong_actor);
    int i = 0;
    int flags = 0;
    future_send(self, std::chrono::seconds(1), atom("FooBar"));
    // wait for :Down and :Exit messages of pong
    receive_while([&i]() { return ++i <= 3; })
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
    */
    await_all_others_done();
    return CPPA_TEST_RESULT;
}
