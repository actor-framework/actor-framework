#include <iostream>
#include <functional>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"

using std::cout;
using std::endl;

using namespace cppa;

void pong()
{
    receive(on<int>() >> [](int value) {
        reply((value * 20) + 2);
    });
}

void echo(actor_ptr whom, int what)
{
    send(whom, what);
}

size_t test__spawn()
{
    CPPA_TEST(test__spawn);
    actor_ptr self_ptr = self();
    {
        auto sl = spawn(pong);
        spawn(echo, self_ptr, 1);
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
    return CPPA_TEST_RESULT;
}
