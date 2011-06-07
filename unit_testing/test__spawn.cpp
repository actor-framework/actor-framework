#include <iostream>
#include <functional>

#include "test.hpp"
#include "ping_pong.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exit_reason.hpp"

using std::cerr;
using std::cout;
using std::endl;

using namespace cppa;

size_t test__spawn()
{
    CPPA_TEST(test__spawn);
    auto report_unexpected = [&] ()
    {
        cerr << "unexpected message: " << to_string(last_received()) << endl;
        CPPA_CHECK(false);
    };
    auto pong_actor = spawn(pong, spawn(ping));
    monitor(pong_actor);
    auto pattern =
    (
        on<atom(":Down"), std::uint32_t>() >> [&] (std::uint32_t reason)
        {
            CPPA_CHECK_EQUAL(reason, exit_reason::user_defined);
            CPPA_CHECK_EQUAL(last_received().sender(), pong_actor);
        },
        others() >> [&] ()
        {
            report_unexpected();
        }
    );
    // wait for :Down message of pong
    receive(pattern);
    // wait for termination of all spawned actors
    await_all_others_done();
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
