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


#include <memory>
#include <iostream>

#include "test.hpp"
#include "cppa/cppa.hpp"

using namespace std;
using namespace cppa;

namespace { constexpr size_t message_size = sizeof(atom_value) + sizeof(int); }

void ping(size_t num_pings) {
    auto count = std::make_shared<size_t>(0);
    become (
        on(atom("kickoff"), arg_match) >> [=](const actor_ptr& pong) {
            send(pong, atom("ping"), 1);
            become (
                on(atom("pong"), arg_match) >> [=](int value) {
                    if (++*count >= num_pings) self->quit();
                    else reply(atom("ping"), value + 1);
                },
                others() >> CPPA_UNEXPECTED_MSG_CB()
            );
        },
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
}

void pong() {
    become  (
        on(atom("ping"), arg_match) >> [](int value) {
            self->monitor(self->last_sender());
            reply(atom("pong"), value);
            become (
                on(atom("ping"), arg_match) >> [](int value) {
                    reply(atom("pong"), value);
                },
                on(atom("DOWN"), arg_match) >> [=](uint32_t reason) {
                    self->quit(reason);
                },
                others() >> CPPA_UNEXPECTED_MSG_CB()
            );
        },
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
}

void peer(io::broker* thisptr, io::connection_handle hdl, const actor_ptr& buddy) {
    self->monitor(buddy);
    if (thisptr->num_connections() == 0) {
        cout << "num_connections() != 1" << endl;
        throw std::logic_error("num_connections() != 1");
    }
    auto write = [=](atom_value type, int value) {
        thisptr->write(hdl, sizeof(type), &type);
        thisptr->write(hdl, sizeof(value), &value);
    };
    become (
        on(atom("IO_closed"), arg_match) >> [=](io::connection_handle) {
            self->quit();
        },
        on(atom("IO_read"), arg_match) >> [=](io::connection_handle, const util::buffer& buf) {
            atom_value type;
            int value;
            memcpy(&type, buf.data(), sizeof(atom_value));
            memcpy(&value, buf.offset_data(sizeof(atom_value)), sizeof(int));
            send(buddy, type, value);
        },
        on(atom("ping"), arg_match) >> [=](int value) {
            write(atom("ping"), value);
        },
        on(atom("pong"), arg_match) >> [=](int value) {
            write(atom("pong"), value);
        },
        on(atom("DOWN"), arg_match) >> [=](uint32_t reason) {
            if (thisptr->last_sender() == buddy) self->quit(reason);
        },
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
}

void peer_acceptor(io::broker* thisptr, const actor_ptr& buddy) {
    become (
        on(atom("IO_accept"), arg_match) >> [=](io::accept_handle, io::connection_handle hdl) {
            CPPA_CHECKPOINT();
            CPPA_LOGF_INFO("received IO_accept");
            thisptr->fork(peer, hdl, buddy);
            self->quit();
        },
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
}

int main(int argc, char** argv) {
    CPPA_TEST(test_broker);
    string app_path = argv[0];
    if (argc == 3) {
        if (strcmp(argv[1], "mode=client") == 0) {
            CPPA_CHECKPOINT();
            run_client_part(get_kv_pairs(argc, argv), [](uint16_t port) {
                CPPA_CHECKPOINT();
                auto p = spawn(ping, 10);
                CPPA_CHECKPOINT();
                auto cl = spawn_io(peer, "localhost", port, p);
                CPPA_CHECKPOINT();
                send_as(nullptr, p, atom("kickoff"), cl);
                CPPA_CHECKPOINT();
            });
            CPPA_CHECKPOINT();
            return CPPA_TEST_RESULT();
        }
        return CPPA_TEST_RESULT();
    }
    else if (argc > 1) {
        cerr << "usage: " << app_path << " [mode=client port={PORT}]" << endl;
        return -1;
    }
    CPPA_CHECKPOINT();
    auto p = spawn(pong);
    uint16_t port = 4242;
    for (;;) {
        try {
            spawn_io_server(peer_acceptor, port, p);
            CPPA_CHECKPOINT();
            ostringstream oss;
            oss << app_path << " mode=client port=" << port << " &>/dev/null";
            thread child{[&oss] {
                CPPA_LOGC_TRACE("NONE", "main$thread_launcher", "");
                auto cmdstr = oss.str();
                if (system(cmdstr.c_str()) != 0) {
                    CPPA_PRINTERR("FATAL: command failed: " << cmdstr);
                    abort();
                }
            }};
            CPPA_CHECKPOINT();
            child.join();
            CPPA_CHECKPOINT();
            await_all_others_done();
            CPPA_CHECKPOINT();
            shutdown();
            return CPPA_TEST_RESULT();
        }
        catch (bind_failure&) {
            // try next port
            ++port;
        }
    }
}
