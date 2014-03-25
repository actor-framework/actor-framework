/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include "cppa/cppa.hpp"

#include "test.hpp"

using namespace std;
using namespace cppa;

namespace {

/******************************************************************************
 *                        simple request/response test                        *
 ******************************************************************************/

struct my_request { int a; int b; };

typedef typed_actor<replies_to<my_request>::with<bool>> server_type;

bool operator==(const my_request& lhs, const my_request& rhs) {
    return lhs.a == rhs.a && lhs.b == rhs.b;
}

server_type::behavior_type typed_server1() {
    return {
        [](const my_request& req) {
            return req.a == req.b;
        }
    };
}

server_type::behavior_type typed_server2(server_type::pointer) {
    return typed_server1();
}

class typed_server3 : public server_type::base {

 public:

    typed_server3(const string& line, actor buddy) {
        send(buddy, line);
    }

    behavior_type make_behavior() override {
        return typed_server2(this);
    }

};

void client(event_based_actor* self, actor parent, server_type serv) {
    self->sync_send(serv, my_request{0, 0}).then(
        [](bool value) -> int {
            CPPA_CHECK_EQUAL(value, true);
            return 42;
        }
    )
    .continue_with([=](int ival) {
        CPPA_CHECK_EQUAL(ival, 42);
        self->sync_send(serv, my_request{10, 20}).then(
            [=](bool value) {
                CPPA_CHECK_EQUAL(value, false);
                self->send(parent, atom("passed"));
            }
        );
    });
}

void test_typed_spawn(server_type ts) {
    scoped_actor self;
    self->send(ts, my_request{1, 2});
    self->receive(
        on_arg_match >> [](bool value) {
            CPPA_CHECK_EQUAL(value, false);
        }
    );
    self->send(ts, my_request{42, 42});
    self->receive(
        on_arg_match >> [](bool value) {
            CPPA_CHECK_EQUAL(value, true);
        }
    );
    self->sync_send(ts, my_request{10, 20}).await(
        [](bool value) {
            CPPA_CHECK_EQUAL(value, false);
        }
    );
    self->sync_send(ts, my_request{0, 0}).await(
        [](bool value) {
            CPPA_CHECK_EQUAL(value, true);
        }
    );
    self->spawn<monitored>(client, self, ts);
    self->receive(
        on(atom("passed")) >> CPPA_CHECKPOINT_CB()
    );
    self->receive(
        on_arg_match >> [](const down_msg& dmsg) {
            CPPA_CHECK_EQUAL(dmsg.reason, exit_reason::normal);
        }
    );
    self->send_exit(ts, exit_reason::user_shutdown);
}

/******************************************************************************
 *          test skipping of messages intentionally + using become()          *
 ******************************************************************************/

struct get_state_msg { };

typedef typed_actor<
            replies_to<get_state_msg>::with<string>,
            replies_to<string>::with<void>,
            replies_to<float>::with<void>,
            replies_to<int>::with<int>
        >
        event_testee_type;

class event_testee : public event_testee_type::base {

 public:

    behavior_type wait4string() {
        return {
            on<get_state_msg>() >> [] {
                return "wait4string";
            },
            on<string>() >> [=] {
                become(wait4int());
            },
            (on<float>() || on<int>()) >> skip_message
        };
    }

    behavior_type wait4int() {
        return {
            on<get_state_msg>() >> [] {
                return "wait4int";
            },
            on<int>() >> [=]() -> int {
                become(wait4float());
                return 42;
            },
            (on<float>() || on<string>()) >> skip_message
        };
    }

    behavior_type wait4float() {
        return {
            on<get_state_msg>() >> [] {
                return "wait4float";
            },
            on<float>() >> [=] {
                become(wait4string());
            },
            (on<string>() || on<int>()) >> skip_message
        };
    }

    behavior_type make_behavior() override {
        return wait4int();
    }

};

void test_event_testee() {
    scoped_actor self;
    auto et = self->spawn_typed<event_testee>();
    string result;
    self->send(et, 1);
    self->send(et, 2);
    self->send(et, 3);
    self->send(et, .1f);
    self->send(et, "hello event testee!");
    self->send(et, .2f);
    self->send(et, .3f);
    self->send(et, "hello again event testee!");
    self->send(et, "goodbye event testee!");
    typed_actor<replies_to<get_state_msg>::with<string>> sub_et = et;
    set<string> iface{"cppa::replies_to<get_state_msg>::with<@str>",
                      "cppa::replies_to<@str>::with<void>",
                      "cppa::replies_to<float>::with<void>",
                      "cppa::replies_to<@i32>::with<@i32>"};
    CPPA_CHECK(sub_et->interface() == iface);
    self->send(sub_et, get_state_msg{});
    // we expect three 42s
    int i = 0;
    self->receive_for(i, 3) (
        [](int value) {
            CPPA_CHECK_EQUAL(value, 42);
        }
    );
    self->receive (
        [&](const string& str) {
            result = str;
        },
        after(chrono::minutes(1)) >> [&]() {
            CPPA_LOGF_ERROR("event_testee does not reply");
            throw runtime_error("event_testee does not reply");
        }
    );
    self->send_exit(et, exit_reason::user_shutdown);
    self->await_all_other_actors_done();
    CPPA_CHECK_EQUAL(result, "wait4int");
}

/******************************************************************************
 *                         simple 'forwarding' chain                          *
 ******************************************************************************/

typedef typed_actor<replies_to<string>::with<string>> string_actor;

void simple_relay(string_actor::pointer self, string_actor master, bool leaf) {
    string_actor next = leaf ? spawn_typed(simple_relay, master, false) : master;
    self->link_to(next);
    self->become(
        on_arg_match >> [=](const string& str) {
            return self->sync_send(next, str).then(
                [](const string& answer) -> string {
                    return answer;
                }
            );
        }
    );
}

string_actor::behavior_type simple_string_reverter() {
    return {
        on_arg_match >> [](const string& str) {
            return string{str.rbegin(), str.rend()};
        }
    };
}

void test_simple_string_reverter() {
    scoped_actor self;
    // actor-under-test
    auto aut = self->spawn_typed<monitored>(simple_relay,
                                            spawn_typed(simple_string_reverter),
                                            true);
    set<string> iface{"cppa::replies_to<@str>::with<@str>"};
    CPPA_CHECK(aut->interface() == iface);
    self->sync_send(aut, "Hello World!").await(
        [](const string& answer) {
            CPPA_CHECK_EQUAL(answer, "!dlroW olleH");
        }
    );
    anon_send_exit(aut, exit_reason::user_shutdown);
}

/******************************************************************************
 *                        sending typed actor handles                         *
 ******************************************************************************/

typedef typed_actor<replies_to<int>::with<int>> int_actor;

int_actor::behavior_type int_fun() {
    return {
        on_arg_match >> [](int i) {
            return i * i;
        }
    };
}

behavior foo(event_based_actor* self) {
    return {
        on_arg_match >> [=](int i, int_actor server) {
            return self->sync_send(server, i).then(
                [=](int result) -> int {
                    self->quit(exit_reason::normal);
                    return result;
                }
            );
        }
    };
}

void test_sending_typed_actors() {
    scoped_actor self;
    auto aut = spawn_typed(int_fun);
    self->send(spawn(foo), 10, aut);
    self->receive(
        on_arg_match >> [](int i) {
            CPPA_CHECK_EQUAL(i, 100);
        }
    );
    self->send_exit(aut, exit_reason::user_shutdown);
}

} // namespace <anonymous>

/******************************************************************************
 *                            put it all together                             *
 ******************************************************************************/

int main() {
    CPPA_TEST(test_typed_spawn);

    // announce stuff
    announce<get_state_msg>();
    announce<int_actor>();
    announce<my_request>(&my_request::a, &my_request::b);

    // run test series with typed_server(1|2)
    test_typed_spawn(spawn_typed(typed_server1));
    CPPA_CHECKPOINT();
    await_all_actors_done();
    CPPA_CHECKPOINT();
    test_typed_spawn(spawn_typed(typed_server2));
    CPPA_CHECKPOINT();
    await_all_actors_done();
    CPPA_CHECKPOINT();
    {
        scoped_actor self;
        test_typed_spawn(spawn_typed<typed_server3>("hi there", self));
        self->receive(
            on("hi there") >> CPPA_CHECKPOINT_CB()
        );
    }
    CPPA_CHECKPOINT();
    await_all_actors_done();

    // run test series with event_testee
    test_event_testee();
    CPPA_CHECKPOINT();
    await_all_actors_done();

    // run test series with string reverter
    test_simple_string_reverter();
    CPPA_CHECKPOINT();
    await_all_actors_done();

    // run test series with sending of typed actors
    test_sending_typed_actors();
    CPPA_CHECKPOINT();
    await_all_actors_done();

    // call it a day
    shutdown();
    return CPPA_TEST_RESULT();
}
