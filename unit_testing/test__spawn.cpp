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

void testee1()
{
    receive_loop
    (
        others() >> []()
        {
            message msg = last_received();
            actor_ptr sender = msg.sender();
            sender->enqueue(message(self(), sender, msg.content()));
        },
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
            reply(sleep_time * 2);
        }
    );
}

size_t test__spawn()
{
    CPPA_TEST(test__spawn);
    auto report_unexpected = [&]()
    {
        cerr << "unexpected message: " << to_string(last_received()) << endl;
        CPPA_CHECK(false);
    };
    trap_exit(true);
    auto pong_actor = spawn(pong, spawn(ping));
    monitor(pong_actor);
    link(pong_actor);
    monitor(spawn(testee2, spawn(testee1)));
    int i = 0;
    int flags = 0;
    get_scheduler()->future_send(self(), std::chrono::seconds(1),
                                 atom("FooBar"));
    // wait for :Down and :Exit messages of pong
    receive_while([&i]() { return ++i <= 4; })
    (
        on<atom(":Exit"), std::uint32_t>() >> [&](std::uint32_t reason)
        {
            CPPA_CHECK_EQUAL(reason, exit_reason::user_defined);
            CPPA_CHECK_EQUAL(last_received().sender(), pong_actor);
            flags |= 0x01;
        },
        on<atom(":Down"), std::uint32_t>() >> [&](std::uint32_t reason)
        {
            CPPA_CHECK_EQUAL(reason, exit_reason::user_defined);
            if (last_received().sender() == pong_actor)
            {
                flags |= 0x02;
            }
            else
            {
                flags |= 0x04;
            }
        },
        on<atom("FooBar")>() >> [&]()
        {
            flags |= 0x08;
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
    CPPA_CHECK_EQUAL(flags, 0x0F);
    // mailbox has to be empty
    message msg;
    while (try_receive(msg))
    {
        report_unexpected();
    }
    // verify pong messages
    CPPA_CHECK_EQUAL(pongs(), 5);
    return CPPA_TEST_RESULT;
}
