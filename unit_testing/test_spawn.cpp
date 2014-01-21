#include <stack>
#include <chrono>
#include <iostream>
#include <functional>

#include "test.hpp"
#include "ping_pong.hpp"

#include "cppa/cppa.hpp"

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
            on<string>() >> [=] {
                become(wait4int);
            },
            on<atom("get_state")>() >> [=] {
                return "wait4string";
            }
        );

        wait4float = (
            on<float>() >> [=] {
                become(wait4string);
            },
            on<atom("get_state")>() >> [=] {
                return "wait4float";
            }
        );

        wait4int = (
            on<int>() >> [=] {
                become(wait4float);
            },
            on<atom("get_state")>() >> [=] {
                return "wait4int";
            }
        );
    }

};

// quits after 5 timeouts
actor spawn_event_testee2(actor parent) {
    struct impl : untyped_actor {
        actor parent;
        impl(actor parent) : parent(parent) { }
        behavior wait4timeout(int remaining) {
            CPPA_LOG_TRACE(CPPA_ARG(remaining));
            return {
                after(chrono::milliseconds(50)) >> [=] {
                    if (remaining == 1) {
                        send(parent, atom("t2done"));
                        quit();
                    }
                    else become(wait4timeout(remaining - 1));
                }
            };
        }
        behavior make_behavior() override {
            return wait4timeout(5);
        }
    };
    return spawn<impl>(parent);
}

struct chopstick : public sb_actor<chopstick> {

    behavior taken_by(actor whom) {
        return (
            on<atom("take")>() >> [=] {
                return atom("busy");
            },
            on(atom("put"), whom) >> [=] {
                become(available);
            },
            on(atom("break")) >> [=] {
                quit();
            }
        );
    }

    behavior available;

    behavior& init_state = available;

    chopstick() {
        available = (
            on(atom("take"), arg_match) >> [=](actor whom) -> atom_value {
                become(taken_by(whom));
                return atom("taken");
            },
            on(atom("break")) >> [=] {
                quit();
            }
        );
    }

};

class testee_actor {

    void wait4string(blocking_untyped_actor* self) {
        bool string_received = false;
        self->do_receive (
            on<string>() >> [&] {
                string_received = true;
            },
            on<atom("get_state")>() >> [&] {
                return "wait4string";
            }
        )
        .until(gref(string_received));
    }

    void wait4float(blocking_untyped_actor* self) {
        bool float_received = false;
        self->do_receive (
            on<float>() >> [&] {
                float_received = true;
            },
            on<atom("get_state")>() >> [&] {
                return "wait4float";
            }
        )
        .until(gref(float_received));
        wait4string(self);
    }

 public:

    void operator()(blocking_untyped_actor* self) {
        self->receive_loop (
            on<int>() >> [&] {
                wait4float(self);
            },
            on<atom("get_state")>() >> [&] {
                return "wait4int";
            }
        );
    }

};

// self->receives one timeout and quits
void testee1(untyped_actor* self) {
    CPPA_LOGF_TRACE("");
    self->become(after(chrono::milliseconds(10)) >> [=] {
        CPPA_LOGF_TRACE("");
        self->unbecome();
    });
}

void testee2(untyped_actor* self, actor other) {
    self->link_to(other);
    self->send(other, uint32_t(1));
    self->become (
        on<uint32_t>() >> [=](uint32_t sleep_time) {
            // "sleep" for sleep_time milliseconds
            self->become (
                keep_behavior,
                after(chrono::milliseconds(sleep_time)) >> [=] {
                    self->unbecome();
                }
            );
        }
    );
}

template<class Testee>
string behavior_test(scoped_actor& self, actor et) {
    string testee_name = detail::to_uniform_name(typeid(Testee));
    CPPA_LOGF_TRACE(CPPA_TARG(et, to_string) << ", " << CPPA_ARG(testee_name));
    string result;
    self->send(et, 1);
    self->send(et, 2);
    self->send(et, 3);
    self->send(et, .1f);
    self->send(et, "hello " + testee_name);
    self->send(et, .2f);
    self->send(et, .3f);
    self->send(et, "hello again " + testee_name);
    self->send(et, "goodbye " + testee_name);
    self->send(et, atom("get_state"));
    self->receive (
        on_arg_match >> [&](const string& str) {
            result = str;
        },
        after(chrono::minutes(1)) >> [&]() {
            CPPA_LOGF_ERROR(testee_name << " does not reply");
            throw runtime_error(testee_name + " does not reply");
        }
    );
    self->send_exit(et, exit_reason::user_shutdown);
    self->await_all_other_actors_done();
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
            on(atom("push"), arg_match) >> [=](int) { /* discard */ },
            on(atom("pop")) >> [=]() -> cow_tuple<atom_value, int> {
                auto result = data.back();
                data.pop_back();
                become(filled);
                return {atom("ok"), result};
            }
        );
        filled = (
            on(atom("push"), arg_match) >> [=](int what) {
                data.push_back(what);
                if (data.size() == max_size) become(full);
            },
            on(atom("pop")) >> [=]() -> cow_tuple<atom_value, int> {
                auto result = data.back();
                data.pop_back();
                if (data.empty()) become(empty);
                return {atom("ok"), result};
            }
        );
        empty = (
            on(atom("push"), arg_match) >> [=](int what) {
                data.push_back(what);
                become(filled);
            },
            on(atom("pop")) >> [=] {
                return atom("failure");
            }
        );

    }

};

behavior echo_actor(untyped_actor* self) {
    return (
        others() >> [=]() -> any_tuple {
            self->quit(exit_reason::normal);
            return self->last_dequeued();
        }
    );
}


struct simple_mirror : sb_actor<simple_mirror> {

    behavior init_state = (
        others() >> [=] {
            return last_dequeued();
        }
    );

};

behavior high_priority_testee(untyped_actor* self) {
    self->send(self, atom("b"));
    self->send(message_priority::high, self, atom("a"));
    // 'a' must be self->received before 'b'
    return (
        on(atom("b")) >> [=] {
            CPPA_FAILURE("received 'b' before 'a'");
            self->quit();
        },
        on(atom("a")) >> [=] {
            CPPA_CHECKPOINT();
            self->become (
                on(atom("b")) >> [=] {
                    CPPA_CHECKPOINT();
                    self->quit();
                },
                others() >> CPPA_UNEXPECTED_MSG_CB()
            );
        },
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
}

struct high_priority_testee_class : untyped_actor {
    behavior make_behavior() override {
        return high_priority_testee(this);
    }
};

struct master : untyped_actor {
    behavior make_behavior() override {
        return (
            on(atom("done")) >> [=] {
                quit(exit_reason::user_shutdown);
            }
        );
    }
};

struct slave : untyped_actor {

    slave(actor master) : master{master} { }

    behavior make_behavior() override {
        link_to(master);
        return (
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );
    }

    actor master;

};

void test_serial_reply() {
    auto mirror_behavior = [=](untyped_actor* self) {
        self->become(others() >> [=]() -> any_tuple {
            CPPA_LOGF_INFO("return self->last_dequeued()");
            return self->last_dequeued();
        });
    };
    auto master = spawn([=](untyped_actor* self) {
        cout << "ID of master: " << self->id() << endl;
        // spawn 5 mirror actors
        auto c0 = self->spawn<linked>(mirror_behavior);
        auto c1 = self->spawn<linked>(mirror_behavior);
        auto c2 = self->spawn<linked>(mirror_behavior);
        auto c3 = self->spawn<linked>(mirror_behavior);
        auto c4 = self->spawn<linked>(mirror_behavior);
        self->become (
          on(atom("hi there")) >> [=]() -> continue_helper {
            CPPA_LOGF_INFO("received 'hi there'");
            return self->sync_send(c0, atom("sub0")).then(
              on(atom("sub0")) >> [=]() -> continue_helper {
                CPPA_LOGF_INFO("received 'sub0'");
                return self->sync_send(c1, atom("sub1")).then(
                  on(atom("sub1")) >> [=]() -> continue_helper {
                    CPPA_LOGF_INFO("received 'sub1'");
                    return self->sync_send(c2, atom("sub2")).then(
                      on(atom("sub2")) >> [=]() -> continue_helper {
                        CPPA_LOGF_INFO("received 'sub2'");
                        return self->sync_send(c3, atom("sub3")).then(
                          on(atom("sub3")) >> [=]() -> continue_helper {
                            CPPA_LOGF_INFO("received 'sub3'");
                            return self->sync_send(c4, atom("sub4")).then(
                              on(atom("sub4")) >> [=]() -> atom_value {
                                CPPA_LOGF_INFO("received 'sub4'");
                                return atom("hiho");
                            }
                          );
                        }
                      );
                    }
                  );
                }
              );
            }
          );
        }
      );
    });
    { // lifetime scope of self
        scoped_actor self;
        cout << "ID of main: " << self->id() << endl;
        self->sync_send(master, atom("hi there")).await(
            on(atom("hiho")) >> [] {
                CPPA_CHECKPOINT();
            },
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );
        self->send_exit(master, exit_reason::user_shutdown);
    }
    await_all_actors_done();
}

void test_or_else() {
    scoped_actor self;
    partial_function handle_a {
        on("a") >> [] { return 1; }
    };
    partial_function handle_b {
        on("b") >> [] { return 2; }
    };
    partial_function handle_c {
        on("c") >> [] { return 3; }
    };
    auto run_testee([&](actor testee) {
        self->sync_send(testee, "a").await([](int i) {
            CPPA_CHECK_EQUAL(i, 1);
        });
        self->sync_send(testee, "b").await([](int i) {
            CPPA_CHECK_EQUAL(i, 2);
        });
        self->sync_send(testee, "c").await([](int i) {
            CPPA_CHECK_EQUAL(i, 3);
        });
        self->send_exit(testee, exit_reason::user_shutdown);
        self->await_all_other_actors_done();

    });
    CPPA_LOGF_INFO("run_testee: handle_a.or_else(handle_b).or_else(handle_c)");
    run_testee(
        spawn([=] {
            return handle_a.or_else(handle_b).or_else(handle_c);
        })
    );
    CPPA_LOGF_INFO("run_testee: handle_a.or_else(handle_b), on(\"c\") ...");
    run_testee(
        spawn([=] {
            return (
                handle_a.or_else(handle_b),
                on("c") >> [] { return 3; }
            );
        })
    );
    CPPA_LOGF_INFO("run_testee: on(\"a\") ..., handle_b.or_else(handle_c)");
    run_testee(
        spawn([=] {
            return (
                on("a") >> [] { return 1; },
                handle_b.or_else(handle_c)
            );
        })
    );
}

void test_continuation() {
    auto mirror = spawn<simple_mirror>();
    spawn([=](untyped_actor* self) {
        self->sync_send(mirror, 42).then(
            on(42) >> [] {
                return "fourty-two";
            }
        ).continue_with(
            [=](const string& ref) {
                CPPA_CHECK_EQUAL(ref, "fourty-two");
                return 4.2f;
            }
        ).continue_with(
            [=](float f) {
                CPPA_CHECK_EQUAL(f, 4.2f);
                self->send_exit(mirror, exit_reason::user_shutdown);
                self->quit();
            }
        );
    });
    await_all_actors_done();
}

void test_simple_reply_response() {
    scoped_actor self;
    auto s = spawn([](untyped_actor* self) -> behavior {
        return (
            others() >> [=]() -> any_tuple {
                CPPA_CHECK(self->last_dequeued() == make_any_tuple(atom("hello")));
                self->quit();
                return self->last_dequeued();
            }
        );
    });
    self->send(s, atom("hello"));
    self->receive(
        others() >> [&] {
            CPPA_CHECK(self->last_dequeued() == make_any_tuple(atom("hello")));
        }
    );
    self->await_all_other_actors_done();
}

void test_spawn() {
    test_simple_reply_response();
    test_serial_reply();
    test_or_else();
    test_continuation();

    scoped_actor self;
    // check whether detached actors and scheduled actors interact w/o errors
    auto m = spawn<master, detached>();
    spawn<slave>(m);
    spawn<slave>(m);
    self->send(m, atom("done"));
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();

    CPPA_PRINT("test self->send()");
    self->send(self, 1, 2, 3, true);
    self->receive(on(1, 2, 3, true) >> [] { });
    self->send_tuple(self, any_tuple{});
    self->receive(on() >> [] { });
    CPPA_CHECKPOINT();

    CPPA_PRINT("test self->receive with zero timeout");
    self->receive (
        others() >> CPPA_UNEXPECTED_MSG_CB(),
        after(chrono::seconds(0)) >> [] { /* mailbox empty */ }
    );
    CPPA_CHECKPOINT();

    CPPA_PRINT("test mirror"); {
        auto mirror = self->spawn<simple_mirror, monitored>();
        self->send(mirror, "hello mirror");
        self->receive (
            on("hello mirror") >> CPPA_CHECKPOINT_CB(),
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );
        self->send_exit(mirror, exit_reason::user_shutdown);
        self->receive (
            on(atom("DOWN"), exit_reason::user_shutdown) >> CPPA_CHECKPOINT_CB(),
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );
        self->await_all_other_actors_done();
        CPPA_CHECKPOINT();
    }

    CPPA_PRINT("test detached mirror"); {
        auto mirror = self->spawn<simple_mirror, monitored+detached>();
        self->send(mirror, "hello mirror");
        self->receive (
            on("hello mirror") >> CPPA_CHECKPOINT_CB(),
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );
        self->send_exit(mirror, exit_reason::user_shutdown);
        self->receive (
            on(atom("DOWN"), exit_reason::user_shutdown) >> CPPA_CHECKPOINT_CB(),
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );
        self->await_all_other_actors_done();
        CPPA_CHECKPOINT();
    }

    CPPA_PRINT("test priority aware mirror"); {
        auto mirror = self->spawn<simple_mirror, monitored+priority_aware>();
        CPPA_CHECKPOINT();
        self->send(mirror, "hello mirror");
        self->receive (
            on("hello mirror") >> CPPA_CHECKPOINT_CB(),
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );
        self->send_exit(mirror, exit_reason::user_shutdown);
        self->receive (
            on(atom("DOWN"), exit_reason::user_shutdown) >> CPPA_CHECKPOINT_CB(),
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );
        self->await_all_other_actors_done();
        CPPA_CHECKPOINT();
    }

    CPPA_PRINT("test echo actor");
    auto mecho = spawn(echo_actor);
    self->send(mecho, "hello echo");
    self->receive (
        on("hello echo") >> [] { },
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();

    CPPA_PRINT("test delayed_send()");
    self->delayed_send(self, chrono::seconds(1), 1, 2, 3);
    self->receive(on(1, 2, 3) >> [] { });
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();

    CPPA_PRINT("test timeout");
    self->receive(after(chrono::seconds(1)) >> [] { });
    CPPA_CHECKPOINT();

    spawn(testee1);
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();

    spawn_event_testee2(self);
    self->receive(on(atom("t2done")) >> CPPA_CHECKPOINT_CB());
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();

    auto cstk = spawn<chopstick>();
    self->send(cstk, atom("take"), self);
    self->receive (
        on(atom("taken")) >> [&] {
            self->send(cstk, atom("put"), self);
            self->send(cstk, atom("break"));
        },
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();

    auto st = spawn<fixed_stack>(10);
    // push 20 values
    for (int i = 0; i < 20; ++i) self->send(st, atom("push"), i);
    // pop 20 times
    for (int i = 0; i < 20; ++i) self->send(st, atom("pop"));
    // expect 10 failure messages
    {
        int i = 0;
        self->receive_for(i, 10) (
            on(atom("failure")) >> CPPA_CHECKPOINT_CB()
        );
        CPPA_CHECKPOINT();
    }
    // expect 10 {'ok', value} messages
    {
        vector<int> values;
        int i = 0;
        self->receive_for(i, 10) (
            on(atom("ok"), arg_match) >> [&](int value) {
                values.push_back(value);
            }
        );
        vector<int> expected{9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
        CPPA_CHECK_EQUAL(util::join(values, ","), util::join(expected, ","));
    }
    // terminate st
    self->send_exit(st, exit_reason::user_shutdown);
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();

    auto sync_testee1 = spawn<blocking_api>([](blocking_untyped_actor* self) {
        self->receive (
            on(atom("get")) >> [] {
                return make_cow_tuple(42, 2);
            }
        );
    });
    self->send(self, 0, 0);
    auto handle = self->sync_send(sync_testee1, atom("get"));
    // wait for some time (until sync response arrived in mailbox)
    self->receive (after(chrono::milliseconds(50)) >> [] { });
    // enqueue async messages (must be skipped by self->receive_response)
    self->send(self, 42, 1);
    // must skip sync message
    self->receive (
        on(42, arg_match) >> [&](int i) {
            CPPA_CHECK_EQUAL(i, 1);
        }
    );
    // must skip remaining async message
    self->receive_response (handle) (
        on_arg_match >> [&](int a, int b) {
            CPPA_CHECK_EQUAL(a, 42);
            CPPA_CHECK_EQUAL(b, 2);
        },
        others() >> CPPA_UNEXPECTED_MSG_CB(),
        after(chrono::seconds(10)) >> CPPA_UNEXPECTED_TOUT_CB()
    );
    // dequeue remaining async. message
    self->receive (on(0, 0) >> CPPA_CHECKPOINT_CB());
    // make sure there's no other message in our mailbox
    self->receive (
        others() >> CPPA_UNEXPECTED_MSG_CB(),
        after(chrono::seconds(0)) >> [] { }
    );
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();

    CPPA_PRINT("test sync send");

    CPPA_CHECKPOINT();
    auto sync_testee = spawn<blocking_api>([](blocking_untyped_actor* self) {
        self->receive (
            on("hi", arg_match) >> [&](actor from) {
                self->sync_send(from, "whassup?", self).await(
                    on_arg_match >> [&](const string& str) -> string {
                        CPPA_CHECK(self->last_sender() != nullptr);
                        CPPA_CHECK_EQUAL(str, "nothing");
                        return "goodbye!";
                    },
                    after(chrono::minutes(1)) >> [] {
                        cerr << "PANIC!!!!" << endl;
                        abort();
                    }
                );
            },
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );
    });
    self->monitor(sync_testee);
    self->send(sync_testee, "hi", self);
    self->receive (
        on("whassup?", arg_match) >> [&](actor other) -> std::string {
            CPPA_CHECKPOINT();
            // this is NOT a reply, it's just an asynchronous message
            self->send(other, "a lot!");
            return "nothing";
        }
    );
    self->receive (
        on("goodbye!") >> CPPA_CHECKPOINT_CB(),
        after(std::chrono::seconds(5)) >> CPPA_UNEXPECTED_TOUT_CB()
    );
    self->receive (
        on(atom("DOWN"), exit_reason::normal) >> [&] {
            CPPA_CHECK_EQUAL(self->last_sender(), sync_testee);
        }
    );
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();

    self->sync_send(sync_testee, "!?").await(
        on(atom("EXITED"), any_vals) >> CPPA_CHECKPOINT_CB(),
        others() >> CPPA_UNEXPECTED_MSG_CB(),
        after(chrono::milliseconds(5)) >> CPPA_UNEXPECTED_TOUT_CB()
    );

    CPPA_CHECKPOINT();

    auto inflater = [](untyped_actor* self, const string& name, actor buddy) {
        CPPA_LOGF_TRACE(CPPA_ARG(self) << ", " << CPPA_ARG(name)
                        << ", " << CPPA_TARG(buddy, to_string));
        self->become(
            on_arg_match >> [=](int n, const string& s) {
                self->send(buddy, n * 2, s);
            },
            on(atom("done")) >> [=] {
                self->quit();
            }
        );
    };
    auto joe = spawn(inflater, "Joe", self);
    auto bob = spawn(inflater, "Bob", joe);
    self->send(bob, 1, "hello actor");
    self->receive (
        on(4, "hello actor") >> CPPA_CHECKPOINT_CB(),
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    // kill joe and bob
    auto poison_pill = make_any_tuple(atom("done"));
    anon_send_tuple(joe, poison_pill);
    anon_send_tuple(bob, poison_pill);
    self->await_all_other_actors_done();

    function<actor (const string&, const actor&)> spawn_next;
    // it's safe to capture spawn_next as reference here, because
    // - it is guaranteeed to outlive kr34t0r by general scoping rules
    // - the lambda is always executed in the current actor's thread
    // but using spawn_next in a message handler could
    // still cause undefined behavior!
    auto kr34t0r = [&spawn_next](untyped_actor* self, const string& name, actor pal) {
        if (name == "Joe" && !pal) {
            pal = spawn_next("Bob", self);
        }
        self->become (
            others() >> [=] {
                // forward message and die
                self->send_tuple(pal, self->last_dequeued());
                self->quit();
            }
        );
    };
    spawn_next = [&kr34t0r](const string& name, const actor& pal) {
        return spawn(kr34t0r, name, pal);
    };
    auto joe_the_second = spawn(kr34t0r, "Joe", invalid_actor);
    self->send(joe_the_second, atom("done"));
    self->await_all_other_actors_done();

    auto f = [](const string& name) -> behavior {
        return (
            on(atom("get_name")) >> [name] {
                return make_cow_tuple(atom("name"), name);
            }
        );
    };
    auto a1 = spawn(f, "alice");
    auto a2 = spawn(f, "bob");
    self->send(a1, atom("get_name"));
    self->receive (
        on(atom("name"), arg_match) >> [&](const string& name) {
            CPPA_CHECK_EQUAL(name, "alice");
        }
    );
    self->send(a2, atom("get_name"));
    self->receive (
        on(atom("name"), arg_match) >> [&](const string& name) {
            CPPA_CHECK_EQUAL(name, "bob");
        }
    );
    self->send_exit(a1, exit_reason::user_shutdown);
    self->send_exit(a2, exit_reason::user_shutdown);
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();

    auto res1 = behavior_test<testee_actor>(self, spawn<blocking_api>(testee_actor{}));
    CPPA_CHECK_EQUAL("wait4int", res1);
    CPPA_CHECK_EQUAL(behavior_test<event_testee>(self, spawn<event_testee>()), "wait4int");

    // create some actors linked to one single actor
    // and kill them all through killing the link
    auto legion = spawn([](untyped_actor* self) {
        CPPA_LOGF_INFO("spawn 1, 000 actors");
        for (int i = 0; i < 1000; ++i) {
            self->spawn<event_testee, linked>();
        }
        self->become(others() >> CPPA_UNEXPECTED_MSG_CB());
    });
    self->send_exit(legion, exit_reason::user_shutdown);
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();
    self->trap_exit(true);
    auto ping_actor = self->spawn<monitored+blocking_api>(ping, 10);
    auto pong_actor = self->spawn<monitored+blocking_api>(pong, ping_actor);
    self->link_to(pong_actor);
    int i = 0;
    int flags = 0;
    self->delayed_send(self, chrono::seconds(1), atom("FooBar"));
    // wait for DOWN and EXIT messages of pong
    self->receive_for(i, 4) (
        on(atom("EXIT"), arg_match) >> [&](uint32_t reason) {
            CPPA_CHECK_EQUAL(exit_reason::user_shutdown, reason);
            CPPA_CHECK(self->last_sender() == pong_actor);
            flags |= 0x01;
        },
        on(atom("DOWN"), arg_match) >> [&](uint32_t reason) {
            auto who = self->last_sender();
            if (who == pong_actor) {
                flags |= 0x02;
                CPPA_CHECK_EQUAL(reason, exit_reason::user_shutdown);
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
            CPPA_FAILURE("unexpected message: " << to_string(self->last_dequeued()));
        },
        after(chrono::seconds(5)) >> [&]() {
            CPPA_FAILURE("timeout in file " << __FILE__ << " in line " << __LINE__);
        }
    );
    // wait for termination of all spawned actors
    self->await_all_other_actors_done();
    CPPA_CHECK_EQUAL(flags, 0x0F);
    // verify pong messages
    CPPA_CHECK_EQUAL(pongs(), 10);
    CPPA_CHECKPOINT();
    spawn<priority_aware>(high_priority_testee);
    self->await_all_other_actors_done();
    CPPA_CHECKPOINT();
    spawn<high_priority_testee_class, priority_aware>();
    self->await_all_other_actors_done();
    // don't try this at home, kids
    self->send(self, atom("check"));
    self->receive (
        on(atom("check")) >> [] {
            CPPA_CHECKPOINT();
        }
    );
}

int main() {
    CPPA_TEST(test_spawn);
    test_spawn();
    shutdown();
    return CPPA_TEST_RESULT();
}
