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

struct my_request { int a; int b; };

bool operator==(const my_request& lhs, const my_request& rhs) {
    return lhs.a == rhs.a && lhs.b == rhs.b;
}

/*
typed_actor_ptr<replies_to<my_request>::with<bool>>
spawn_typed_server() {
    return spawn_typed(
        on_arg_match >> [](const my_request& req) {
            return req.a == req.b;
        }
    );
}

class typed_testee : public typed_actor<replies_to<my_request>::with<bool>> {

 protected:

    behavior_type make_behavior() override {
        return (
            on_arg_match >> [](const my_request& req) {
                return req.a == req.b;
            }
        );
    }

};
*/

int main() {
    CPPA_TEST(test_typed_spawn);
/*
    announce<my_request>(&my_request::a, &my_request::b);
    auto sptr = spawn_typed_server();
    sync_send(sptr, my_request{2, 2}).await(
        [](bool value) {
            CPPA_CHECK_EQUAL(value, true);
        }
    );
    send_exit(sptr, exit_reason::user_shutdown);
    sptr = spawn_typed<typed_testee>();
    sync_send(sptr, my_request{2, 2}).await(
        [](bool value) {
            CPPA_CHECK_EQUAL(value, true);
        }
    );
    send_exit(sptr, exit_reason::user_shutdown);
    auto ptr0 = spawn_typed(
        on_arg_match >> [](double d) {
            return d * d;
        },
        on_arg_match >> [](float f) {
            return f / 2.0f;
        }
    );
    CPPA_CHECK((std::is_same<
                    decltype(ptr0),
                    typed_actor_ptr<
                        replies_to<double>::with<double>,
                        replies_to<float>::with<float>
                    >
                >::value));
    typed_actor_ptr<replies_to<double>::with<double>> ptr0_double = ptr0;
    typed_actor_ptr<replies_to<float>::with<float>> ptr0_float = ptr0;
    auto ptr = spawn_typed(
        on<int>() >> [] { return "wtf"; },
        on<string>() >> [] { return 42; },
        on<float>() >> [] { return make_cow_tuple(1, 2, 3); },
        on<double>() >> [=](double d) {
            return sync_send(ptr0_double, d).then(
                [](double res) { return res + res; }
            );
        }
    );
    // check async messages
    send(ptr0_float, 4.0f);
    receive(
        on_arg_match >> [](float f) {
            CPPA_CHECK_EQUAL(f, 4.0f / 2.0f);
        }
    );
    // check sync messages
    sync_send(ptr0_float, 4.0f).await(
        [](float f) {
            CPPA_CHECK_EQUAL(f, 4.0f / 2.0f);
        }
    );
    sync_send(ptr, 10.0).await(
        [](double d) {
            CPPA_CHECK_EQUAL(d, 200.0);
        }
    );
    sync_send(ptr, 42).await(
        [](const std::string& str) {
            CPPA_CHECK_EQUAL(str, "wtf");
        }
    );
    sync_send(ptr, 1.2f).await(
        [](int a, int b, int c) {
            CPPA_CHECK_EQUAL(a, 1);
            CPPA_CHECK_EQUAL(b, 2);
            CPPA_CHECK_EQUAL(c, 3);
        }
    );
    sync_send(ptr, 1.2f).await(
        [](int b, int c) {
            CPPA_CHECK_EQUAL(b, 2);
            CPPA_CHECK_EQUAL(c, 3);
        }
    );
    sync_send(ptr, 1.2f).await(
        [](int c) {
            CPPA_CHECK_EQUAL(c, 3);
        }
    );
    sync_send(ptr, 1.2f).await(
        [] { CPPA_CHECKPOINT(); }
    );
    spawn([=] {
        sync_send(ptr, 2.3f).then(
            [] (int c) {
                CPPA_CHECK_EQUAL(c, 3);
                return "hello continuation";
            }
        ).continue_with(
            [] (const string& str) {
                CPPA_CHECK_EQUAL(str, "hello continuation");
                return 4.2;
            }
        ).continue_with(
            [=] (double d) {
                CPPA_CHECK_EQUAL(d, 4.2);
                send_exit(ptr0, exit_reason::user_shutdown);
                send_exit(ptr,  exit_reason::user_shutdown);
                self->quit();
            }
        );
    });
    await_all_actors_done();
    CPPA_CHECKPOINT();
    shutdown();
*/
    return CPPA_TEST_RESULT();
}
