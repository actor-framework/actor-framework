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
 * Copyright (C) 2011-2014                                                    *
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


#include <vector>
#include <string>
#include <limits>
#include <memory>
#include <iostream>

#include <arpa/inet.h>

#include "cppa/cppa.hpp"
#include "cppa/logging.hpp"
#include "cppa/singletons.hpp"

#include "cppa/io/broker.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/tcp_acceptor.hpp"
#include "cppa/io/tcp_io_stream.hpp"

CPPA_PUSH_WARNINGS
#include "pingpong.pb.h"
CPPA_POP_WARNINGS

using namespace std;
using namespace cppa;
using namespace cppa::io;

void print_on_exit(const actor& ptr, const std::string& name) {
    ptr->attach_functor([=](std::uint32_t reason) {
        aout(ptr) << name << " exited with reason " << reason << endl;
    });
}

behavior ping(event_based_actor* self, size_t num_pings) {
    auto count = make_shared<size_t>(0);
    return {
        on(atom("kickoff"), arg_match) >> [=](const actor& pong) {
            self->send(pong, atom("ping"), 1);
            self->become (
                on(atom("pong"), arg_match) >> [=](int value) -> any_tuple {
                    if (++*count >= num_pings) self->quit();
                    return make_any_tuple(atom("ping"), value + 1);
                }
            );
        }
    };
}

behavior pong() {
    return {
        on(atom("ping"), arg_match) >> [](int value) {
            return make_any_tuple(atom("pong"), value);
        }
    };
}

void protobuf_io(broker* self, connection_handle hdl, const actor& buddy) {
    self->monitor(buddy);
    auto write = [=](const org::libcppa::PingOrPong& p) {
        string buf = p.SerializeAsString();
        int32_t s = htonl(static_cast<int32_t>(buf.size()));
        self->write(hdl, sizeof(int32_t), &s);
        self->write(hdl, buf.size(), buf.data());
    };
    partial_function default_bhvr = {
        [=](const connection_closed_msg&) {
            aout(self) << "connection closed" << endl;
            self->send_exit(buddy, exit_reason::remote_link_unreachable);
            self->quit(exit_reason::remote_link_unreachable);
        },
        on(atom("ping"), arg_match) >> [=](int i) {
            aout(self) << "'ping' " << i << endl;
            org::libcppa::PingOrPong p;
            p.mutable_ping()->set_id(i);
            write(p);
        },
        on(atom("pong"), arg_match) >> [=](int i) {
            aout(self) << "'pong' " << i << endl;
            org::libcppa::PingOrPong p;
            p.mutable_pong()->set_id(i);
            write(p);
        },
        [=](const down_msg& dm) {
            if (dm.source == buddy) {
                aout(self) << "our buddy is down" << endl;
                self->quit(dm.reason);
            }
        },
        others() >> [=] {
            cout << "unexpected: " << to_string(self->last_dequeued()) << endl;
        }
    };
    partial_function await_protobuf_data {
        [=](const new_data_msg& msg) {
            org::libcppa::PingOrPong p;
            p.ParseFromArray(msg.buf.data(), static_cast<int>(msg.buf.size()));
            if (p.has_ping()) {
                self->send(buddy, atom("ping"), p.ping().id());
            }
            else if (p.has_pong()) {
                self->send(buddy, atom("pong"), p.pong().id());
            }
            else {
                self->quit(exit_reason::user_shutdown);
                cerr << "neither Ping nor Pong!" << endl;
            }
            // receive next length prefix
            self->receive_policy(hdl, broker::exactly, sizeof(int32_t));
            self->unbecome();
        },
        default_bhvr
    };
    partial_function await_length_prefix {
        [=](const new_data_msg& msg) {
            int32_t num_bytes;
            memcpy(&num_bytes, msg.buf.data(), sizeof(int32_t));
            num_bytes = htonl(num_bytes);
            if (num_bytes < 0 || num_bytes > (1024 * 1024)) {
                aout(self) << "someone is trying something nasty" << endl;
                self->quit(exit_reason::user_shutdown);
                return;
            }
            // receive protobuf data
            self->receive_policy(hdl, broker::exactly, static_cast<size_t>(num_bytes));
            self->become(keep_behavior, await_protobuf_data);
        },
        default_bhvr
    };
    // initial setup
    self->receive_policy(hdl, broker::exactly, sizeof(int32_t));
    self->become(await_length_prefix);
}

behavior server(broker* self, actor buddy) {
    aout(self) << "server is running" << endl;
    return {
        [=](const new_connection_msg& msg) {
            aout(self) << "server accepted new connection" << endl;
            auto io_actor = self->fork(protobuf_io, msg.handle, buddy);
            print_on_exit(io_actor, "protobuf_io");
            // only accept 1 connection in our example
            self->quit();
        },
        others() >> [=] {
            cout << "unexpected: " << to_string(self->last_dequeued()) << endl;
        }
    };
}

optional<uint16_t> as_u16(const std::string& str) {
    return static_cast<uint16_t>(stoul(str));
}

int main(int argc, char** argv) {
    match(std::vector<string>{argv + 1, argv + argc}) (
        on("-s", as_u16) >> [&](uint16_t port) {
            cout << "run in server mode" << endl;
            auto pong_actor = spawn(pong);
            auto sever_actor = spawn_io_server(server, port, pong_actor);
            print_on_exit(sever_actor, "server");
            print_on_exit(pong_actor, "pong");
        },
        on("-c", val<string>, as_u16) >> [&](const string& host, uint16_t port) {
            auto ping_actor = spawn(ping, 20);
            auto io_actor = spawn_io_client(protobuf_io, host, port, ping_actor);
            print_on_exit(io_actor, "protobuf_io");
            print_on_exit(ping_actor, "ping");
            send_as(io_actor, ping_actor, atom("kickoff"), io_actor);
        },
        others() >> [] {
            cerr << "use with eihter '-s PORT' as server or '-c HOST PORT' as client"
                 << endl;
        }
    );
    await_all_actors_done();
    shutdown();
}
