#include "cppa/config.hpp"

#include <map>
#include <list>
#include <mutex>
#include <iostream>
#include <algorithm>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/self.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

using std::cout;
using std::endl;

using std::string;
using namespace cppa;

size_t test__local_group() {
    CPPA_TEST(test__local_group);
    auto foo_group = group::get("local", "foo");
    actor_ptr master = self;
    for (int i = 0; i < 5; ++i) {
        // spawn five workers and let them join local/foo
        spawn_in_group(foo_group, [master]() {
            receive(on<int>() >> [master](int v) {
                send(master, v);
            });
        });
    }
    send(foo_group, 2);
    int result = 0;
    do_receive (
        on<int>() >> [&](int value) {
            CPPA_CHECK(value == 2);
            result += value;
        },
        after(std::chrono::seconds(2)) >> [&]() {
            CPPA_CHECK(false);
            result = 10;
        }
    )
    .until(gref(result) == 10);
    await_all_others_done();
    return CPPA_TEST_RESULT;
}
