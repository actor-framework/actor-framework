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
#include "cppa/io/ipv4_acceptor.hpp"
#include "cppa/io/ipv4_io_stream.hpp"

#include "pingpong.pb.h"

using namespace std;
using namespace cppa;
using namespace cppa::io;

void print_on_exit(const actor_ptr& ptr, const std::string& name) {
    ptr->attach_functor([=](std::uint32_t reason) {
        aout << name << " exited with reason " << reason << endl;
    });
}

void ping(size_t num_pings) {
    auto count = make_shared<size_t>(0);
    become  (
        on(atom("kickoff"), arg_match) >> [=](const actor_ptr& pong) {
            send(pong, atom("ping"), 1);
            become (
                on(atom("pong"), arg_match) >> [=](int value) -> any_tuple {
                    aout << "pong: " << value << endl;
                    if (++*count >= num_pings) self->quit();
                    return {atom("ping"), value + 1};
                }
            );
        }
    );
}

void pong() {
    become  (
        on(atom("ping"), arg_match) >> [](int value) {
            aout << "ping: " << value << endl;
            reply(atom("pong"), value);
        }
    );
}

void protobuf_io(broker* thisptr, connection_handle hdl, const actor_ptr& buddy) {
    self->monitor(buddy);
    auto write = [=](const org::libcppa::PingOrPong& p) {
            string buf = p.SerializeAsString();
            int32_t s = htonl(static_cast<int32_t>(buf.size()));
            thisptr->write(hdl, sizeof(int32_t), &s);
            thisptr->write(hdl, buf.size(), buf.data());
    };
    auto default_bhvr = (
        on(atom("IO_closed"), hdl) >> [=] {
            cout << "IO_closed" << endl;
            send_exit(buddy, exit_reason::remote_link_unreachable);
            self->quit(exit_reason::remote_link_unreachable);
        },
        on(atom("ping"), arg_match) >> [=](int i) {
            org::libcppa::PingOrPong p;
            p.mutable_ping()->set_id(i);
            write(p);
        },
        on(atom("pong"), arg_match) >> [=](int i) {
            org::libcppa::PingOrPong p;
            p.mutable_pong()->set_id(i);
            write(p);
        },
        on(atom("DOWN"), arg_match) >> [=](uint32_t rsn) {
            if (self->last_sender() == buddy) self->quit(rsn);
        },
        others() >> [=] {
            cout << "unexpected: " << to_string(self->last_dequeued()) << endl;
        }
    );
    partial_function await_protobuf_data {
        on(atom("IO_read"), hdl, arg_match) >> [=](const util::buffer& buf) {
            org::libcppa::PingOrPong p;
            p.ParseFromArray(buf.data(), static_cast<int>(buf.size()));
            if (p.has_ping()) {
                send(buddy, atom("ping"), p.ping().id());
            }
            else if (p.has_pong()) {
                send(buddy, atom("pong"), p.pong().id());
            }
            else {
                self->quit(exit_reason::user_shutdown);
                cerr << "neither Ping nor Pong!" << endl;
            }
            // receive next length prefix
            thisptr->receive_policy(hdl, broker::exactly, 4);
            unbecome();
        },
        default_bhvr
    };
    partial_function await_length_prefix {
        on(atom("IO_read"), hdl, arg_match) >> [=](const util::buffer& buf) {
            int32_t num_bytes;
            memcpy(&num_bytes, buf.data(), 4);
            num_bytes = htonl(num_bytes);
            if (num_bytes < 0 || num_bytes > (1024 * 1024)) {
                aout << "someone is trying something nasty" << endl;
                self->quit(exit_reason::user_shutdown);
                return;
            }
            // receive protobuf data
            thisptr->receive_policy(hdl, broker::exactly, static_cast<size_t>(num_bytes));
            become(keep_behavior, await_protobuf_data);
        },
        default_bhvr
    };
    // initial setup
    thisptr->receive_policy(hdl, broker::exactly, 4);
    become(await_length_prefix);
}

void server(broker* thisptr, actor_ptr buddy) {
    aout << "server is running" << endl;
    become (
        on(atom("IO_accept"), arg_match) >> [=](accept_handle, connection_handle hdl) {
            aout << "server: IO_accept" << endl;
            auto io_actor = thisptr->fork(protobuf_io, hdl, buddy);
            print_on_exit(io_actor, "protobuf_io");
            // only accept 1 connection
            thisptr->quit();
        },
        others() >> [=] {
            cout << "unexpected: " << to_string(self->last_dequeued()) << endl;
        }
    );
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
            auto io_actor = spawn_io(protobuf_io, host, port, ping_actor);
            print_on_exit(io_actor, "protobuf_io");
            print_on_exit(ping_actor, "ping");
            send_as(io_actor, ping_actor, atom("kickoff"), io_actor);
        },
        others() >> [] {
            cerr << "use with eihter '-s PORT' as server or '-c HOST PORT' as client"
                 << endl;
        }
    );
    await_all_others_done();
    shutdown();
}
