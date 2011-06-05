#include <iostream>
#include <functional>

#include "test.hpp"
#include "ping_pong.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/to_string.hpp"

using std::cout;
using std::endl;

using namespace cppa;

size_t test__spawn()
{
    CPPA_TEST(test__spawn);
    spawn(pong, spawn(ping));
    await_all_others_done();
    CPPA_CHECK_EQUAL(pongs(), 5);
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
