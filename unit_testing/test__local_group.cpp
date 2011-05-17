#include "cppa/config.hpp"

#include <map>
#include <list>
#include <mutex>
#include <iostream>
#include <algorithm>

#include "test.hpp"
#include "hash_of.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

using std::cout;
using std::endl;

using namespace cppa;

void worker()
{
    receive(on<int>() >> [](int value) {
        reply(value);
    });
}

size_t test__local_group()
{
    CPPA_TEST(test__local_group);
    auto foo_group = group::get("local", "foo");
    for (int i = 0; i < 5; ++i)
    {
        // spawn workers and let them join local/foo
        spawn(worker)->join(foo_group);
    }
    send(foo_group, 2);
    int result = 0;
    for (int i = 0; i < 5; ++i)
    {
        receive(on<int>() >> [&result](int value) { result += value; });
    }
    CPPA_CHECK_EQUAL(result, 10);
    await_all_others_done();
    return CPPA_TEST_RESULT;
}
