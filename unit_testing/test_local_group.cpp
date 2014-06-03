#include "cppa/config.hpp"

#include <map>
#include <list>
#include <mutex>
#include <iostream>
#include <algorithm>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/abstract_group.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

using std::cout;
using std::endl;

using std::string;
using namespace cppa;

void testee(event_based_actor* self, int current_value, int final_result) {
    self->become(
        [=](int result) {
            auto next = result + current_value;
            if (next >= final_result) {
                CPPA_CHECKPOINT();
                self->quit();
            }
            else testee(self, next, final_result);
        },
        after(std::chrono::seconds(2)) >> [=] {
            CPPA_UNEXPECTED_TOUT();
            self->quit(exit_reason::user_shutdown);
        }
    );
}

void test_local_group() {
    scoped_actor self;
    auto foo_group = group::get("local", "foo");
    auto master = spawn_in_group(foo_group, testee, 0, 10);
    for (int i = 0; i < 5; ++i) {
        // spawn five workers and let them join local/foo
        self->spawn_in_group(foo_group, [master](event_based_actor* m) {
            m->become([master, m](int v) {
                m->send(master, v);
                m->quit();
            });
        });
    }
    self->send(foo_group, 2);
}

int main() {
    CPPA_TEST(test_local_group);
    test_local_group();
    await_all_actors_done();
    shutdown();
    return CPPA_TEST_RESULT();
}
