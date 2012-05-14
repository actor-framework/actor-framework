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
#if 0 //(__GNUC__ >= 4) && (__GNUC_MINOR__ >= 7)

struct event_testee : public fsm_actor<event_testee> {

    behavior wait4string = (
        on<std::string>() >> [=]() {
            become(&wait4int);
        },
        on<atom("get_state")>() >> [=]() {
            reply("wait4string");
        }
    );

    behavior wait4float = (
        on<float>() >> [=]() {
            become(&wait4string);
        },
        on<atom("get_state")>() >> [=]() {
            reply("wait4float");
        }
    );

    behavior wait4int = (
        on<int>() >> [=]() {
            become(&wait4float);
        },
        on<atom("get_state")>() >> [=]() {
            reply("wait4int");
        }
    );

    behavior& init_state = wait4int;

};

#else

class event_testee : public fsm_actor<event_testee> {

    friend class fsm_actor<event_testee>;

    behavior wait4string;
    behavior wait4float;
    behavior wait4int;

    behavior& init_state;

 public:

    event_testee() : init_state(wait4int) {
        wait4string = (
            on<std::string>() >> [=]() {
                become(&wait4int);
            },
            on<atom("get_state")>() >> [=]() {
                reply("wait4string");
            }
        );

        wait4float = (
            on<float>() >> [=]() {
                become(&wait4string);
            },
            on<atom("get_state")>() >> [=]() {
                reply("wait4float");
            }
        );

        wait4int = (
            on<int>() >> [=]() {
                become(&wait4float);
            },
            on<atom("get_state")>() >> [=]() {
                reply("wait4int");
            }
        );
    }

};

#endif

// quits after 5 timeouts
abstract_event_based_actor* event_testee2() {
    struct impl : fsm_actor<impl> {
        behavior wait4timeout(int remaining) {
            return (
                after(std::chrono::milliseconds(50)) >> [=]() {
                    if (remaining == 1) become_void();
                    else become(wait4timeout(remaining - 1));
                }
            );
        }

        behavior init_state;

        impl() : init_state(wait4timeout(5)) { }
    };
    return new impl;
}

struct chopstick : public fsm_actor<chopstick> {

    behavior taken_by(actor_ptr hakker) {
        return (
            on<atom("take")>() >> [=]() {
                reply(atom("busy"));
            },
            on(atom("put"), hakker) >> [=]() {
                become(&init_state);
            },
            on(atom("break")) >> [=]() {
                become_void();
            }
        );
    }

    behavior init_state;

    chopstick() {
        init_state = (
            on(atom("take"), arg_match) >> [=](actor_ptr hakker) {
                become(taken_by(hakker));
                reply(atom("taken"));
            },
            on(atom("break")) >> [=]() {
                become_void();
            },
            others() >> [=]() {
            }
        );
    }

};

class testee_actor {

    void wait4string() {
        bool string_received = false;
        do_receive (
            on<std::string>() >> [&]() {
                string_received = true;
            },
            on<atom("get_state")>() >> [&]() {
                reply("wait4string");
            }
        )
        .until(gref(string_received));
    }

    void wait4float() {
        bool float_received = false;
        do_receive (
            on<float>() >> [&]() {
                float_received = true;
                wait4string();
            },
            on<atom("get_state")>() >> [&]() {
                reply("wait4float");
            }
        )
        .until(gref(float_received));
    }

 public:

    void operator()() {
        receive_loop (
            on<int>() >> [&]() {
                wait4float();
            },
            on<atom("get_state")>() >> [&]() {
                reply("wait4int");
            }
        );
    }

};

// receives one timeout and quits
void testee1() {
    receive (
        after(std::chrono::milliseconds(10)) >> []() { }
    );
}

void testee2(actor_ptr other) {
    self->link_to(other);
    send(other, std::uint32_t(1));
    receive_loop (
        on<std::uint32_t>() >> [](std::uint32_t sleep_time) {
            // "sleep" for sleep_time milliseconds
            receive(after(std::chrono::milliseconds(sleep_time)) >> []() {});
            //reply(sleep_time * 2);
        }
    );
}

void testee3(actor_ptr parent) {
    // test a future_send / delayed_reply based loop
    future_send(self, std::chrono::milliseconds(50), atom("Poll"));
    int polls = 0;
    receive_for(polls, 5) (
        on(atom("Poll")) >> [&]() {
            if (polls < 4) {
                delayed_reply(std::chrono::milliseconds(50), atom("Poll"));
            }
            send(parent, atom("Push"), polls);
        }
    );
}

void echo_actor() {
    receive (
        others() >> []() {
            self->last_sender() << self->last_dequeued();
        }
    );
}

template<class Testee>
std::string behavior_test(actor_ptr et) {
    std::string result;
    std::string testee_name = detail::to_uniform_name(typeid(Testee));
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
    receive (
        on_arg_match >> [&](const std::string& str) {
            result = str;
        },
        after(std::chrono::seconds(2)) >> [&]() {
            throw std::runtime_error(testee_name + " does not reply");
        }
    );
    send(et, atom("EXIT"), exit_reason::user_defined);
    await_all_others_done();
    return result;
}

class str_wrapper {

    str_wrapper() = delete;
    str_wrapper(str_wrapper&&) = delete;
    str_wrapper(const str_wrapper&) = delete;

 public:

    inline str_wrapper(std::string s) : m_str(s) { }

    const std::string& str() const {
        return m_str;
    }

 private:

    std::string m_str;

};

bool operator==(const str_wrapper& lhs, const std::string& rhs) {
    return lhs.str() == rhs;
}

void foobar(const str_wrapper& x, const std::string& y) {
    receive (
        on(atom("same")).when(gref(x) == gref(y)) >> [&]() {
            reply(atom("yes"));
        },
        on(atom("same")) >> [&]() {
            reply(atom("no"));
        }
    );
}

size_t test__spawn() {
    CPPA_TEST(test__spawn);

    CPPA_IF_VERBOSE(cout << "test send() ... " << std::flush);
    send(self, 1, 2, 3);
    receive(on(1, 2, 3) >> []() { });
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "test future_send() ... " << std::flush);
    future_send(self, std::chrono::seconds(1), 1, 2, 3);
    receive(on(1, 2, 3) >> []() { });
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "test timeout ... " << std::flush);
    receive(after(std::chrono::seconds(1)) >> []() { });
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "test echo actor ... " << std::flush);
    auto mecho = spawn(echo_actor);
    send(mecho, "hello echo");
    receive(on("hello echo") >> []() { });
    await_all_others_done();
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "testee1 ... " << std::flush);
    spawn(testee1);
    await_all_others_done();
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "event_testee2 ... " << std::flush);
    spawn(event_testee2());
    await_all_others_done();
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "chopstick ... " << std::flush);
    auto cstk = spawn(new chopstick);
    send(cstk, atom("take"), self);
    receive (
        on(atom("taken")) >> [&]() {
            send(cstk, atom("put"), self);
            send(cstk, atom("break"));
        }
    );
    await_all_others_done();
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    {
        bool invoked = false;
        str_wrapper x{"x"};
        std::string y{"y"};
        auto foo_actor = spawn(foobar, std::cref(x), y);
        send(foo_actor, atom("same"));
        receive (
            on(atom("yes")) >> [&]() {
                CPPA_ERROR("x == y");
            },
            on(atom("no")) >> [&]() {
                invoked = true;
            }
        );
        CPPA_CHECK_EQUAL(true, invoked);
        await_all_others_done();
    }

    CPPA_CHECK_EQUAL(behavior_test<testee_actor>(spawn(testee_actor{})), "wait4int");
    CPPA_CHECK_EQUAL(behavior_test<event_testee>(spawn(new event_testee)), "wait4int");

    // create 20,000 actors linked to one single actor
    // and kill them all through killing the link
    auto my_link = spawn(new event_testee);
    for (int i = 0; i < 20000; ++i) {
        link(my_link, spawn(new event_testee));
    }
    send(my_link, atom("EXIT"), exit_reason::user_defined);
    await_all_others_done();
    self->trap_exit(true);
    auto ping_actor = spawn(ping, 10);
    auto pong_actor = spawn(pong, ping_actor);
    monitor(pong_actor);
    monitor(ping_actor);
    self->link_to(pong_actor);
    int i = 0;
    int flags = 0;
    future_send(self, std::chrono::seconds(1), atom("FooBar"));
    // wait for DOWN and EXIT messages of pong
    receive_for(i, 4) (
        on<atom("EXIT"), std::uint32_t>() >> [&](std::uint32_t reason) {
            CPPA_CHECK_EQUAL(reason, exit_reason::user_defined);
            CPPA_CHECK(self->last_sender() == pong_actor);
            flags |= 0x01;
        },
        on<atom("DOWN"), actor_ptr, std::uint32_t>() >> [&](const actor_ptr& who,
                                                            std::uint32_t reason) {
            if (who == pong_actor) {
                flags |= 0x02;
                CPPA_CHECK_EQUAL(reason, exit_reason::user_defined);
            }
            else if (who == ping_actor) {
                flags |= 0x04;
                CPPA_CHECK_EQUAL(reason, exit_reason::normal);
            }
        },
        on<atom("FooBar")>() >> [&]() {
            flags |= 0x08;
        },
        others() >> [&]() {
            CPPA_ERROR("unexpected message: " << to_string(self->last_dequeued()));
        },
        after(std::chrono::seconds(5)) >> [&]() {
            CPPA_ERROR("timeout in file " << __FILE__ << " in line " << __LINE__);
        }
    );
    // wait for termination of all spawned actors
    await_all_others_done();
    CPPA_CHECK_EQUAL(0x0F, flags);
    // verify pong messages
    CPPA_CHECK_EQUAL(10, pongs());
    return CPPA_TEST_RESULT;

    /****** TODO

    int add_fun(int, int);
    int sub_fun(int, int);

    ...

    class math {
     public:
        int add_fun(int, int);
        int sub_fun(int, int);
    };

    ...

    auto factory1 = actor_prototype (
        on<atom("add"), int, int>().reply_with(atom("result"), add_fun),
        on<atom("sub"), int, int>().reply_with(atom("result"), add_fun)
    );
    auto a1 = factory1.spawn();

    ...

    auto factory2 = actor_facade<math> (
        on<atom("add"), int, int>().reply_with(atom("result"), &math::add_fun), <-- possible?
        on<atom("sub"), int, int>().reply_with(atom("result"), &math::sub_fun)
    );
    auto a2 = factory2.spawn();

    */

}
