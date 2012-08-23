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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
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


#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "utility.hpp"

#include "cppa/cppa.hpp"

using namespace std;
using namespace cppa;

struct testee : sb_actor<testee> {
    behavior init_state;
    testee(const actor_ptr& parent) {
        init_state = (
            on(atom("spread"), (uint32_t) 0) >> [=]() {
                send(parent, atom("result"), (uint32_t) 1);
                quit();
            },
            on(atom("spread"), arg_match) >> [=](uint32_t x) {
                any_tuple msg = make_cow_tuple(atom("spread"), x - 1);
                spawn<testee>(this) << msg;
                spawn<testee>(this) << msg;
                become (
                    on(atom("result"), arg_match) >> [=](uint32_t r1) {
                        become (
                            on(atom("result"), arg_match) >> [=](uint32_t r2) {
                                send(parent, atom("result"), r1 + r2);
                                quit();
                            }
                        );
                    }
                );
            }
        );
    }
};

void stacked_testee(actor_ptr parent) {
    receive (
        on(atom("spread"), (uint32_t) 0) >> [&]() {
            send(parent, atom("result"), (uint32_t) 1);
        },
        on(atom("spread"), arg_match) >> [&](uint32_t x) {
            any_tuple msg = make_cow_tuple(atom("spread"), x - 1);
            spawn(stacked_testee, self) << msg;
            spawn(stacked_testee, self) << msg;
            receive (
                on(atom("result"), arg_match) >> [&](uint32_t v1) {
                    receive (
                        on(atom("result"), arg_match) >> [&](uint32_t v2) {
                            send(parent, atom("result"), v1 + v2);
                        }
                    );
                }
            );
        }
    );
}

void usage() {
    cout << "usage: actor_creation (stacked|event-based) POW" << endl
         << "       creates 2^POW actors" << endl
         << endl;
}

int main(int argc, char** argv) {
    vector<string> args(argv + 1, argv + argc);
    match (args) (
        on("stacked", spro<uint32_t>) >> [](uint32_t num) {
            send(spawn(stacked_testee, self), atom("spread"), num);
        },
        on("event-based", spro<uint32_t>) >> [](uint32_t num) {
            send(spawn<testee>(self), atom("spread"), num);
        },
        others() >> usage
    );
    await_all_others_done();
    shutdown();
    return 0;
}
