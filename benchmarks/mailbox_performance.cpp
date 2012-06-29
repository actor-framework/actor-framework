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


#include <thread>
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
using std::int64_t;

using namespace cppa;

struct fsm_receiver : sb_actor<fsm_receiver> {
    int64_t m_value;
    behavior init_state;
    fsm_receiver(int64_t max) : m_value(0) {
        init_state = (
            on(atom("msg")) >> [=]() {
                ++m_value;
                if (m_value == max) {
                    quit();
                }
            }
        );
    }
};

void receiver(int64_t max) {
    int64_t value;
    receive_while(gref(value) < max) (
        on(atom("msg")) >> [&]() {
            ++value;
        }
    );
}

void sender(actor_ptr whom, int64_t count) {
    any_tuple msg = make_cow_tuple(atom("msg"));
    for (int64_t i = 0; i < count; ++i) {
        whom->enqueue(nullptr, msg);
    }
}

void usage() {
    cout << "usage: mailbox_performance "
            "(stacked|event-based) (sending threads) (msg per thread)" << endl
         << endl;
}

int main(int argc, char** argv) {
    if (argc == 4) {
        int64_t num_sender = rd<int64_t>(argv[2]);
        int64_t num_msgs = rd<int64_t>(argv[3]);
        actor_ptr testee;
        if (strcmp(argv[1], "stacked") == 0) {
            testee = spawn(receiver, num_sender * num_msgs);
        }
        else if (strcmp(argv[1], "event-based") == 0) {
            testee = spawn<fsm_receiver>(num_sender * num_msgs);
        }
        else {
            usage();
            return 1;
        }
        for (int64_t i = 0; i < num_sender; ++i) {
            std::thread(sender, testee, num_msgs).detach();
        }
        await_all_others_done();
    }
    else {
        usage();
        return 1;
    }
    return 0;
}
