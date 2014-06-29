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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#include <memory>
#include <iostream>

#include "test.hpp"
#include "cppa/all.hpp"
#include "cppa/io/all.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::io;

void ping(event_based_actor* self, size_t num_pings) {
    CPPA_PRINT("num_pings: " << num_pings);
    auto count = std::make_shared<size_t>(0);
    self->become(
        on(atom("kickoff"), arg_match) >> [=](const actor& pong) {
            CPPA_CHECKPOINT();
            self->send(pong, atom("ping"), 1);
            self->become(
            on(atom("pong"), arg_match) >>
            [=](int value)->std::tuple<atom_value, int> {
                if (++*count >= num_pings) {
                    CPPA_PRINT("received " << num_pings
                               << " pings, call self->quit");
                    self->quit();
                }
                return std::make_tuple(atom("ping"), value + 1);
            },
            others() >> CPPA_UNEXPECTED_MSG_CB(self));
        },
        others() >> CPPA_UNEXPECTED_MSG_CB(self)
    );
}

void pong(event_based_actor* self) {
    CPPA_CHECKPOINT();
    self->become(
        on(atom("ping"), arg_match) >> [=](int value)
                                           ->std::tuple<atom_value, int> {
            CPPA_CHECKPOINT();
            self->monitor(self->last_sender());
            // set next behavior
            self->become(
                on(atom("ping"), arg_match) >> [](int val) {
                    return std::make_tuple(atom("pong"), val);
                },
                [=](const down_msg& dm) {
                    CPPA_PRINT("received down_msg{" << dm.reason << "}");
                    self->quit(dm.reason);
                },
                others() >> CPPA_UNEXPECTED_MSG_CB(self)
            );
            // reply to 'ping'
            return std::make_tuple(atom("pong"), value);
        },
        others() >> CPPA_UNEXPECTED_MSG_CB(self));
}

void peer_fun(broker* self, connection_handle hdl, const actor& buddy) {
    CPPA_CHECKPOINT();
    CPPA_CHECK(self != nullptr);
    CPPA_CHECK(buddy != invalid_actor);
    self->monitor(buddy);
    // assume exactly one connection
    auto cons = self->connections();
    if (cons.size() != 1) {
        cerr << "expected 1 connection, found " << cons.size() << endl;
        throw std::logic_error("num_connections() != 1");
    }
    self->configure_read(
        hdl, receive_policy::exactly(sizeof(atom_value) + sizeof(int)));
    auto write = [=](atom_value type, int value) {
        CPPA_LOGF_DEBUG("write: " << value);
        auto& buf = self->wr_buf(hdl);
        auto first = reinterpret_cast<char*>(&type);
        buf.insert(buf.end(), first, first + sizeof(atom_value));
        first = reinterpret_cast<char*>(&value);
        buf.insert(buf.end(), first, first + sizeof(int));
        self->flush(hdl);

    };
    self->become(
        [=](const connection_closed_msg&) {
            CPPA_PRINT("received connection_closed_msg");
            self->quit();
        },
        [=](const new_data_msg& msg) {
            CPPA_PRINT("received new_data_msg");
            atom_value type;
            int value;
            memcpy(&type, msg.buf.data(), sizeof(atom_value));
            memcpy(&value, msg.buf.data() + sizeof(atom_value), sizeof(int));
            self->send(buddy, type, value);
        },
        on(atom("ping"), arg_match) >> [=](int value) {
            CPPA_PRINT("received ping{" << value << "}");
            write(atom("ping"), value);
        },
        on(atom("pong"), arg_match) >> [=](int value) {
            CPPA_PRINT("received pong{" << value << "}");
            write(atom("pong"), value);
        },
        [=](const down_msg& dm) {
            CPPA_PRINT("received down_msg");
            if (dm.source == buddy) self->quit(dm.reason);
        },
        others() >> CPPA_UNEXPECTED_MSG_CB(self)
    );
}

behavior peer_acceptor_fun(broker* self, const actor& buddy) {
    CPPA_CHECKPOINT();
    return {
        [=](const new_connection_msg& msg) {
            CPPA_CHECKPOINT();
            CPPA_PRINT("received new_connection_msg");
            self->fork(peer_fun, msg.handle, buddy);
            self->quit();
        },
        others() >> CPPA_UNEXPECTED_MSG_CB(self)
    };
}

void run_server(bool spawn_client, const char* bin_path) {
    auto p = spawn(pong);
    uint16_t port = 4242;
    bool done = false;
    while (!done) {
        try {
            spawn_functor(nullptr,
                          [=](broker* bro) {
                              bro->add_acceptor(
                                  network::new_ipv4_acceptor(port));
                          },
                          peer_acceptor_fun, p);
        }
        catch (bind_failure&) {
            // try next port
            ++port;
        }
        done = true;
    }
    CPPA_CHECKPOINT();
    if (!spawn_client) {
        cout << "server is running on port " << port << endl;
    } else {
        ostringstream oss;
        oss << bin_path << " -c " << port << to_dev_null;
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
    }
}

int main(int argc, char** argv) {
    CPPA_TEST(test_broker);
    message_builder{argv + 1, argv + argc}.apply({
       on("-c", arg_match) >> [&](const std::string& portstr) {
            auto port = static_cast<uint16_t>(std::stoi(portstr));
            auto p = spawn(ping, 10);
            CPPA_CHECKPOINT();
            auto cl = spawn_io_client(peer_fun, "localhost", port, p);
            CPPA_CHECKPOINT();
            anon_send(p, atom("kickoff"), cl);
            CPPA_CHECKPOINT();
        },
        on("-s")  >> [&] {
            run_server(false, argv[0]);
        },
        on() >> [&] {
            run_server(true, argv[0]);

        },
        others() >> [&] {
           cerr << "usage: " << argv[0] << " [-c PORT]" << endl;
        }
    });
    CPPA_CHECKPOINT();
    await_all_actors_done();
    CPPA_CHECKPOINT();
    shutdown();
    return CPPA_TEST_RESULT();
}
