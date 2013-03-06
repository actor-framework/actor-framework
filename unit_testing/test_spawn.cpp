#include <stack>
#include <chrono>
#include <iostream>
#include <functional>

#include "test.hpp"
#include "ping_pong.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/factory.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/sb_actor.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/event_based_actor.hpp"
#include "cppa/util/callable_trait.hpp"

using namespace std;
using namespace cppa;

class event_testee : public sb_actor<event_testee> {

    friend class sb_actor<event_testee>;

    behavior wait4string;
    behavior wait4float;
    behavior wait4int;

    behavior& init_state = wait4int;

 public:

    event_testee() {
        wait4string = (
            on<string>() >> [=]() {
                become(wait4int);
            },
            on<atom("get_state")>() >> [=]() {
                reply("wait4string");
            }
        );

        wait4float = (
            on<float>() >> [=]() {
                become(wait4string);
            },
            on<atom("get_state")>() >> [=]() {
                reply("wait4float");
            }
        );

        wait4int = (
            on<int>() >> [=]() {
                become(wait4float);
            },
            on<atom("get_state")>() >> [=]() {
                reply("wait4int");
            }
        );
    }

};

// quits after 5 timeouts
actor_ptr spawn_event_testee2() {
    struct impl : sb_actor<impl> {
        behavior wait4timeout(int remaining) {
            return (
                after(chrono::milliseconds(50)) >> [=]() {
                    if (remaining == 1) quit();
                    else become(wait4timeout(remaining - 1));
                }
            );
        }

        behavior init_state;

        impl() : init_state(wait4timeout(5)) { }
    };
    return spawn<impl>();
}

struct chopstick : public sb_actor<chopstick> {

    behavior taken_by(actor_ptr whom) {
        return (
            on<atom("take")>() >> [=]() {
                reply(atom("busy"));
            },
            on(atom("put"), whom) >> [=]() {
                become(available);
            },
            on(atom("break")) >> [=]() {
                quit();
            }
        );
    }

    behavior available;

    behavior& init_state = available;

    chopstick() {
        available = (
            on(atom("take"), arg_match) >> [=](actor_ptr whom) {
                become(taken_by(whom));
                reply(atom("taken"));
            },
            on(atom("break")) >> [=]() {
                quit();
            }
        );
    }

};

class testee_actor {

    void wait4string() {
        bool string_received = false;
        do_receive (
            on<string>() >> [&] {
                string_received = true;
            },
            on<atom("get_state")>() >> [&] {
                reply("wait4string");
            }
        )
        .until(gref(string_received));
    }

    void wait4float() {
        bool float_received = false;
        do_receive (
            on<float>() >> [&] {
                float_received = true;
            },
            on<atom("get_state")>() >> [&] {
                reply("wait4float");
            }
        )
        .until(gref(float_received));
        wait4string();
    }

 public:

    void operator()() {
        receive_loop (
            on<int>() >> [&] {
                wait4float();
            },
            on<atom("get_state")>() >> [&] {
                reply("wait4int");
            }
        );
    }

};

// receives one timeout and quits
void testee1() {
    become(after(chrono::milliseconds(10)) >> [] { unbecome(); });
}

void testee2(actor_ptr other) {
    self->link_to(other);
    send(other, uint32_t(1));
    become (
        on<uint32_t>() >> [](uint32_t sleep_time) {
            // "sleep" for sleep_time milliseconds
            become (
                keep_behavior,
                after(chrono::milliseconds(sleep_time)) >> [] { unbecome(); }
            );
        }
    );
}

template<class Testee>
string behavior_test(actor_ptr et) {
    string result;
    string testee_name = detail::to_uniform_name(typeid(Testee));
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
        on_arg_match >> [&](const string& str) {
            result = str;
        },
        after(chrono::minutes(1)) >> [&]() {
        //after(chrono::seconds(2)) >> [&]() {
            throw runtime_error(testee_name + " does not reply");
        }
    );
    quit_actor(et, exit_reason::user_defined);
    await_all_others_done();
    return result;
}

class fixed_stack : public sb_actor<fixed_stack> {

    friend class sb_actor<fixed_stack>;

    size_t max_size = 10;

    vector<int> data;

    behavior full;
    behavior filled;
    behavior empty;

    behavior& init_state = empty;

 public:

    fixed_stack(size_t max) : max_size(max)  {

        full = (
            on(atom("push"), arg_match) >> [=](int) { },
            on(atom("pop")) >> [=]() {
                reply(atom("ok"), data.back());
                data.pop_back();
                become(filled);
            }
        );

        filled = (
            on(atom("push"), arg_match) >> [=](int what) {
                data.push_back(what);
                if (data.size() == max_size)
                    become(full);
            },
            on(atom("pop")) >> [=]() {
                reply(atom("ok"), data.back());
                data.pop_back();
                if (data.empty())
                    become(empty);
            }
        );

        empty = (
            on(atom("push"), arg_match) >> [=](int what) {
                data.push_back(what);
                become(filled);
            },
            on(atom("pop")) >> [=]() {
                reply(atom("failure"));
            }
        );

    }

};

void echo_actor() {
    become (
        others() >> [] {
            reply_tuple(self->last_dequeued());
            self->quit(exit_reason::normal);
        }
    );
}


struct simple_mirror : sb_actor<simple_mirror> {

    behavior init_state = (
        others() >> []() {
            reply_tuple(self->last_dequeued());
        }
    );

};

int main() {
    CPPA_TEST(test__spawn);

    CPPA_PRINT("test send()");
    send(self, 1, 2, 3, true);
    receive(on(1, 2, 3, true) >> []() { });
    self << any_tuple{};
    receive(on() >> []() { });
    CPPA_CHECKPOINT();

    self << any_tuple{};
    receive(on() >> []() { });

    CPPA_PRINT("test receive with zero timeout");
    receive (
        others() >> CPPA_UNEXPECTED_MSG_CB(),
        after(chrono::seconds(0)) >> []() { /* mailbox empty */ }
    );
    CPPA_CHECKPOINT();

    auto mirror = spawn<simple_mirror,monitored>();

    CPPA_PRINT("test mirror");
    send(mirror, "hello mirror");
    receive (
        on("hello mirror") >> CPPA_CHECKPOINT_CB(),
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    quit_actor(mirror, exit_reason::user_defined);
    receive (
        on(atom("DOWN"), exit_reason::user_defined) >> CPPA_CHECKPOINT_CB(),
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    await_all_others_done();
    CPPA_CHECKPOINT();

    CPPA_PRINT("test echo actor");
    auto mecho = spawn(echo_actor);
    send(mecho, "hello echo");
    receive (
        on("hello echo") >> []() { },
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    await_all_others_done();
    CPPA_CHECKPOINT();

    CPPA_PRINT("test delayed_send()");
    delayed_send(self, chrono::seconds(1), 1, 2, 3);
    receive(on(1, 2, 3) >> []() { });
    CPPA_CHECKPOINT();

    CPPA_PRINT("test timeout");
    receive(after(chrono::seconds(1)) >> []() { });
    CPPA_CHECKPOINT();

    spawn(testee1);
    await_all_others_done();
    CPPA_CHECKPOINT();

    spawn_event_testee2();
    await_all_others_done();
    CPPA_CHECKPOINT();

    auto cstk = spawn<chopstick>();
    send(cstk, atom("take"), self);
    receive (
        on(atom("taken")) >> [&]() {
            send(cstk, atom("put"), self);
            send(cstk, atom("break"));
        }
    );
    await_all_others_done();
    CPPA_CHECKPOINT();

    auto factory = factory::event_based([&](int* i, float*, string*) {
        self->become (
            on(atom("get_int")) >> [i]() {
                reply(*i);
            },
            on(atom("set_int"), arg_match) >> [i](int new_value) {
                *i = new_value;
            },
            on(atom("done")) >> []() {
                self->quit();
            }
        );
    });
    auto foobaz_actor = factory.spawn(23);
    send(foobaz_actor, atom("get_int"));
    send(foobaz_actor, atom("set_int"), 42);
    send(foobaz_actor, atom("get_int"));
    send(foobaz_actor, atom("done"));
    receive (
        on_arg_match >> [&](int value) {
            CPPA_CHECK_EQUAL(value, 23);
        }
    );
    receive (
        on_arg_match >> [&](int value) {
            CPPA_CHECK_EQUAL(value, 42);
        }
    );
    await_all_others_done();
    CPPA_CHECKPOINT();

    auto st = spawn<fixed_stack>(10);
    // push 20 values
    for (int i = 0; i < 20; ++i) send(st, atom("push"), i);
    // pop 20 times
    for (int i = 0; i < 20; ++i) send(st, atom("pop"));
    // expect 10 failure messages
    {
        int i = 0;
        receive_for(i, 10) (
            on(atom("failure")) >> [] { }
        );
        CPPA_CHECKPOINT();
    }
    // expect 10 {'ok', value} messages
    {
        vector<int> values;
        int i = 0;
        receive_for(i, 10) (
            on(atom("ok"), arg_match) >> [&](int value) {
                values.push_back(value);
            }
        );
        vector<int> expected{9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
        CPPA_CHECK(values == expected);
    }
    // terminate st
    quit_actor(st, exit_reason::user_defined);
    await_all_others_done();
    CPPA_CHECKPOINT();

    auto sync_testee1 = spawn<blocking_api>([] {
        receive (
            on(atom("get")) >> []() {
                reply(42, 2);
            }
        );
    });
    send(self, 0, 0);
    auto handle = sync_send(sync_testee1, atom("get"));
    // wait for some time (until sync response arrived in mailbox)
    receive (after(chrono::milliseconds(50)) >> []() { });
    // enqueue async messages (must be skipped by receive_response)
    send(self, 42, 1);
    // must skip sync message
    receive (
        on(42, arg_match) >> [&](int i) {
            CPPA_CHECK_EQUAL(i, 1);
        }
    );
    // must skip remaining async message
    receive_response (handle) (
        on_arg_match >> [&](int a, int b) {
            CPPA_CHECK_EQUAL(a, 42);
            CPPA_CHECK_EQUAL(b, 2);
        },
        others() >> CPPA_UNEXPECTED_MSG_CB(),
        after(chrono::seconds(10)) >> CPPA_UNEXPECTED_TOUT_CB()
    );
    // dequeue remaining async. message
    receive (on(0, 0) >> CPPA_CHECKPOINT_CB());
    // make sure there's no other message in our mailbox
    receive (
        others() >> CPPA_UNEXPECTED_MSG_CB(),
        after(chrono::seconds(0)) >> []() { }
    );
    await_all_others_done();
    CPPA_CHECKPOINT();

    CPPA_PRINT("test sync send with factory spawned actor");
    auto sync_testee_factory = factory::event_based(
        [&]() {
            self->become (
                on("hi") >> [&]() {
                    auto handle = sync_send(self->last_sender(), "whassup?");
                    self->handle_response(handle) (
                        on_arg_match >> [&](const string& str) {
                            CPPA_CHECK(self->last_sender() != nullptr);
                            CPPA_CHECK_EQUAL(str, "nothing");
                            reply("goodbye!");
                            self->quit();
                        },
                        after(chrono::minutes(1)) >> []() {
                            cerr << "PANIC!!!!" << endl;
                            abort();
                        }
                    );
                },
                others() >> CPPA_UNEXPECTED_MSG_CB()
            );
        }
    );
    CPPA_CHECKPOINT();
    auto sync_testee = sync_testee_factory.spawn();
    self->monitor(sync_testee);
    send(sync_testee, "hi");
    receive (
        on("whassup?") >> [&] {
            CPPA_CHECK(true);
            // this is NOT a reply, it's just an asynchronous message
            send(self->last_sender(), "a lot!");
            reply("nothing");
        }
    );
    receive (
        on("goodbye!") >> CPPA_CHECKPOINT_CB(),
        after(std::chrono::seconds(5)) >> CPPA_UNEXPECTED_TOUT_CB()
    );
    receive (
        on(atom("DOWN"), exit_reason::normal) >> [&] {
            CPPA_CHECK_EQUAL(self->last_sender(), sync_testee);
        }
    );
    await_all_others_done();
    CPPA_CHECKPOINT();

    sync_send(sync_testee, "!?").await(
        on(atom("EXITED"), any_vals) >> CPPA_CHECKPOINT_CB(),
        others() >> CPPA_UNEXPECTED_MSG_CB(),
        after(chrono::milliseconds(5)) >> CPPA_UNEXPECTED_TOUT_CB()
    );

    auto inflater = factory::event_based(
        [](string*, actor_ptr* receiver) {
            self->become(
                on_arg_match >> [=](int n, const string& s) {
                    send(*receiver, n * 2, s);
                },
                on(atom("done")) >> []() {
                    self->quit();
                }
            );
        }
    );
    auto joe = inflater.spawn("Joe", self);
    auto bob = inflater.spawn("Bob", joe);
    send(bob, 1, "hello actor");
    receive (
        on(4, "hello actor") >> CPPA_CHECKPOINT_CB(),
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    // kill joe and bob
    auto poison_pill = make_any_tuple(atom("done"));
    joe << poison_pill;
    bob << poison_pill;
    await_all_others_done();

    function<actor_ptr (const string&, const actor_ptr&)> spawn_next;
    auto kr34t0r = factory::event_based(
        // it's safe to pass spawn_next as reference here, because
        // - it is guaranteeed to outlive kr34t0r by general scoping rules
        // - the lambda is always executed in the current actor's thread
        // but using spawn_next in a message handler could
        // still cause undefined behavior!
        [&spawn_next](string* name, actor_ptr* pal) {
            if (*name == "Joe" && !*pal) {
                *pal = spawn_next("Bob", self);
            }
            self->become (
                others() >> [pal]() {
                    // forward message and die
                    *pal << self->last_dequeued();
                    self->quit();
                }
            );
        }
    );
    spawn_next = [&kr34t0r](const string& name, const actor_ptr& pal) {
        return kr34t0r.spawn(name, pal);
    };
    auto joe_the_second = kr34t0r.spawn("Joe");
    send(joe_the_second, atom("done"));
    await_all_others_done();

    int zombie_init_called = 0;
    int zombie_on_exit_called = 0;
    factory::event_based([&]() { ++zombie_init_called; },
                         [&]() { ++zombie_on_exit_called; }).spawn();
    CPPA_CHECK_EQUAL(zombie_init_called, 1);
    CPPA_CHECK_EQUAL(zombie_on_exit_called, 1);
    factory::event_based([&](int* i) {
        CPPA_CHECK_EQUAL(*i, 42);
        ++zombie_init_called;
    },
    [&](int* i) {
        CPPA_CHECK_EQUAL(*i, 42);
        ++zombie_on_exit_called;
    })
    .spawn(42);
    CPPA_CHECK_EQUAL(zombie_init_called, 2);
    CPPA_CHECK_EQUAL(zombie_on_exit_called, 2);
    factory::event_based([&](int* i) {
        CPPA_CHECK_EQUAL(*i, 23);
        ++zombie_init_called;
    },
    [&]() {
        ++zombie_on_exit_called;
    })
    .spawn(23);
    CPPA_CHECK_EQUAL(zombie_init_called, 3);
    CPPA_CHECK_EQUAL(zombie_on_exit_called, 3);

    auto f = factory::event_based([](string* name) {
        self->become (
            on(atom("get_name")) >> [name]() {
                reply(atom("name"), *name);
            }
        );
    });
    auto a1 = f.spawn("alice");
    auto a2 = f.spawn("bob");
    send(a1, atom("get_name"));
    receive (
        on(atom("name"), arg_match) >> [&](const string& name) {
            CPPA_CHECK_EQUAL(name, "alice");
        }
    );
    send(a2, atom("get_name"));
    receive (
        on(atom("name"), arg_match) >> [&](const string& name) {
            CPPA_CHECK_EQUAL(name, "bob");
        }
    );
    quit_actor(a1, exit_reason::user_defined);
    quit_actor(a2, exit_reason::user_defined);
    await_all_others_done();

    factory::event_based([](int* i) {
        self->become(
            after(chrono::milliseconds(50)) >> [=]() {
                if (++(*i) >= 5) self->quit();
            }

        );
    }).spawn();
    await_all_others_done();

    auto res1 = behavior_test<testee_actor>(spawn<blocking_api>(testee_actor{}));
    CPPA_CHECK_EQUAL("wait4int", res1);
    CPPA_CHECK_EQUAL(behavior_test<event_testee>(spawn<event_testee>()), "wait4int");

    // create some actors linked to one single actor
    // and kill them all through killing the link
    auto legion = spawn([] {
        CPPA_LOGF_INFO("spawn 1,000 actors");
        for (int i = 0; i < 1000; ++i) {
            spawn<event_testee,linked>();
        }
        become(others() >> CPPA_UNEXPECTED_MSG_CB());
    });
    quit_actor(legion, exit_reason::user_defined);
    await_all_others_done();
    CPPA_CHECKPOINT();
    self->trap_exit(true);
    auto ping_actor = spawn<monitored+blocking_api>(ping, 10);
    auto pong_actor = spawn<monitored+blocking_api>(pong, ping_actor);
    self->link_to(pong_actor);
    int i = 0;
    int flags = 0;
    delayed_send(self, chrono::seconds(1), atom("FooBar"));
    // wait for DOWN and EXIT messages of pong
    receive_for(i, 4) (
        on(atom("EXIT"), arg_match) >> [&](uint32_t reason) {
            CPPA_CHECK_EQUAL(exit_reason::user_defined, reason);
            CPPA_CHECK(self->last_sender() == pong_actor);
            flags |= 0x01;
        },
        on(atom("DOWN"), arg_match) >> [&](uint32_t reason) {
            auto who = self->last_sender();
            if (who == pong_actor) {
                flags |= 0x02;
                CPPA_CHECK_EQUAL(reason, exit_reason::user_defined);
            }
            else if (who == ping_actor) {
                flags |= 0x04;
                CPPA_CHECK_EQUAL(reason, exit_reason::normal);
            }
        },
        on_arg_match >> [&](const atom_value& val) {
            CPPA_CHECK(val == atom("FooBar"));
            flags |= 0x08;
        },
        others() >> [&]() {
            CPPA_ERROR("unexpected message: " << to_string(self->last_dequeued()));
        },
        after(chrono::seconds(5)) >> [&]() {
            CPPA_ERROR("timeout in file " << __FILE__ << " in line " << __LINE__);
        }
    );
    // wait for termination of all spawned actors
    await_all_others_done();
    CPPA_CHECK_EQUAL(flags, 0x0F);
    // verify pong messages
    CPPA_CHECK_EQUAL(pongs(), 10);
    return CPPA_TEST_RESULT();
}
