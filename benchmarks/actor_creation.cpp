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
#include "cppa/sb_actor.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::uint32_t;

using namespace cppa;

struct testee : sb_actor<testee> {
    actor_ptr parent;
    behavior init_state;
    testee(const actor_ptr& pptr) : parent(pptr) {
        init_state = (
            on(atom("spread"), 0) >> [=]() {
                send(parent, atom("result"), (uint32_t) 1);
                quit();
            },
            on<atom("spread"), int>() >> [=](int x) {
                any_tuple msg = make_cow_tuple(atom("spread"), x - 1);
                spawn<testee>(this) << msg;
                spawn<testee>(this) << msg;
                become (
                    on<atom("result"), uint32_t>() >> [=](uint32_t r1) {
                        become (
                            on<atom("result"), uint32_t>() >> [=](uint32_t r2) {
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
        on(atom("spread"), 0) >> [&]() {
            send(parent, atom("result"), (uint32_t) 1);
        },
        on<atom("spread"), int>() >> [&](int x) {
            any_tuple msg = make_cow_tuple(atom("spread"), x-1);
            spawn(stacked_testee, self) << msg;
            spawn(stacked_testee, self) << msg;
            receive (
                on<atom("result"), uint32_t>() >> [&](uint32_t v1) {
                    receive (
                        on<atom("result"),uint32_t>() >> [&](uint32_t v2) {
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
    if (argc == 3) {
        int num = rd<int>(argv[2]);
        if (strcmp(argv[1], "stacked") == 0) {
            send(spawn(stacked_testee, self), atom("spread"), num);
        }
        else if (strcmp(argv[1], "event-based") == 0) {
            send(spawn<testee>(self), atom("spread"), num);
        }
        else {
            usage();
            return 1;
        }
        receive (
            on<atom("result"),uint32_t>() >> [=](uint32_t value) {
                //cout << "value = " << value << endl
                //     << "expected => 2^" << num << " = "
                //     << (1 << num) << endl;
                if (value != (((uint32_t) 1) << num)) {
                    cerr << "ERROR: received wrong result!\n";
                }
            }
        );
        await_all_others_done();
    }
    else {
        usage();
        return 1;
    }
    return 0;
}
