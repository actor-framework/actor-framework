#include <iostream>
#include <functional>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"

using std::cout;
using std::endl;

namespace { int pings = 0; }

using namespace cppa;

void pong(actor_ptr ping_actor)
{
    link(ping_actor);
    bool quit = false;
    // kickoff
    ping_actor << make_tuple(0); // or: send(ping_actor, 0);
    // invoke rules
    auto rules = (on<std::int32_t>(9) >> [&]() { quit = true; },
                  on<std::int32_t>() >> [](int v) { reply(v+1); });
    // loop
    while (!quit) receive(rules);
}

void ping()
{
    // invoke rule
    auto rule = on<std::int32_t>() >> [](std::int32_t v)
    {
        ++pings;
        reply(v+1);
    };
    // loop
    for (;;) receive(rule);
}

void pong_example()
{
    spawn(pong, spawn(ping));
    await_all_others_done();
}

void echo(actor_ptr whom, int what)
{
    send(whom, what);
}

size_t test__spawn()
{
    CPPA_TEST(test__spawn);
    pong_example();
    CPPA_CHECK_EQUAL(pings, 5);
    /*
    actor_ptr self_ptr = self();
    {
        spawn(echo, self_ptr, 1);
        spawn(pong) << make_tuple(23.f) << make_tuple(2);
        //send(sl, 23.f);
        //send(sl, 2);
        bool received_pong = false;
        bool received_echo = false;
        auto rules = (on<int>(42) >> [&]() { received_pong = true; },
                      on<int>(1) >> [&]() { received_echo = true; });
        receive(rules);
        receive(rules);
        CPPA_CHECK(received_pong);
        CPPA_CHECK(received_echo);
    }
    {
        auto sl = spawn([]() {
            receive(on<int>() >> [](int value) {
                reply((value * 20) + 2);
            });
        });
        spawn([](actor_ptr whom, int what) { send(whom, what); },
              self_ptr,
              1);
        send(sl, 23.f);
        send(sl, 2);
        bool received_pong = false;
        bool received_echo = false;
        auto rules = (on<int>(42) >> [&]() { received_pong = true; },
                      on<int>(1) >> [&]() { received_echo = true; });
        receive(rules);
        receive(rules);
        CPPA_CHECK(received_pong);
        CPPA_CHECK(received_echo);
    }
    await_all_others_done();
    */
    return CPPA_TEST_RESULT;
}
