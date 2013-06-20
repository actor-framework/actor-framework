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

#include "cppa/cppa.hpp"
#include "cppa/logging.hpp"
#include "cppa/io/ipv4_acceptor.hpp"
#include "cppa/io/ipv4_io_stream.hpp"

#include "pingpong.pb.h"


#include "cppa/singletons.hpp"
#include "cppa/io/broker.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/io_handle.hpp"
#include "cppa/io/broker_backend.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::network;

void ping(size_t num_pings) {
    auto count = make_shared<size_t>(0);
    become  (
        on(atom("kickoff"), arg_match) >> [=](const actor_ptr& pong) {
            send(pong, atom("ping"), 1);
            become (
                on(atom("pong"), arg_match) >> [=](int value) {
                    cout << "<- pong " << value << endl;
                    if (++*count >= num_pings) self->quit();
                    else reply(atom("ping"), value + 1);
                }
            );
        }
    );
}

void pong() {
    become  (
        on(atom("ping"), arg_match) >> [](int value) {
            cout << "<- ping " << value << endl;
            reply(atom("pong"), value);
        }
    );
}

void protobuf_io(io_handle* ios, const actor_ptr& buddy) {
    self->monitor(buddy);
    auto write = [=](const org::libcppa::PingOrPong& p) {
            string buf = p.SerializeAsString();
            int32_t s = htonl(static_cast<int32_t>(buf.size()));
            ios->write(sizeof(int32_t), &s);
            ios->write(buf.size(), buf.data());
    };
    auto default_bhvr = (
        on(atom("IO_closed"), arg_match) >> [=](uint32_t) {
            cout << "IO_closed" << endl;
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
        on(atom("IO_read"), arg_match) >> [=](uint32_t, const util::buffer& buf) {
            org::libcppa::PingOrPong p;
            p.ParseFromArray(buf.data(), static_cast<int>(buf.size()));
            auto print = [](const char* name, int value) {
                cout << name << "{" << value << "}" << endl;
            };
            if (p.has_ping()) {
                send(buddy, atom("ping"), p.ping().id());
            }
            else if (p.has_pong()) {
                send(buddy, atom("pong"), p.pong().id());
            }
            else {
                self->quit(exit_reason::user_defined);
                cerr << "neither Pong nor Ping!" << endl;
            }
            // receive next length prefix
            ios->receive_policy(io_handle::exactly, 4);
            unbecome();
        },
        default_bhvr
    };
    partial_function await_length_prefix {
        on(atom("IO_read"), arg_match) >> [=](uint32_t, const util::buffer& buf) {
            int num_bytes;
            memcpy(&num_bytes, buf.data(), 4);
            num_bytes = htonl(num_bytes);
            if (num_bytes < 0 || num_bytes > (1024 * 1024)) {
                self->quit(exit_reason::user_defined);
                return;
            }
            // receive protobuf data
            ios->receive_policy(io_handle::exactly, (size_t) num_bytes);
            become(keep_behavior, await_protobuf_data);
        },
        default_bhvr
    };
    // initial setup
    ios->receive_policy(io_handle::exactly, 4);
    become(await_length_prefix);
}

int main(int argc, char** argv) {
    auto print_exit = [](const actor_ptr& ptr, const std::string& name) {
        ptr->attach_functor([=](std::uint32_t reason) {
            cout << name << " exited with reason " << reason << endl;
        });
    };
    match(std::vector<string>{argv + 1, argv + argc}) (
        on("-s") >> [&] {
            cout << "run in server mode" << endl;
            std::string hi = "hello";
            auto po = spawn(pong);
            auto ack = io::ipv4_acceptor::create(4242);
            for (;;) {
                auto p = ack->accept_connection();
                //spawn_io<protobuf_io>(p.first, p.second);
                auto s = spawn_io(protobuf_io, std::move(p.first), std::move(p.second), po);
                print_exit(s, "io actor");
                print_exit(po, "pong");
            }
        },
        on_arg_match >> [&](const string& host, const string& port_str) {
            auto port = static_cast<uint16_t>(stoul(port_str));
            auto io = io::ipv4_io_stream::connect_to(host.c_str(), port);
            auto pi = spawn(ping, 20);
            auto pr = spawn_io(protobuf_io, io, io, pi);
            send_as(pr, pi, atom("kickoff"), pr);
            print_exit(pr, "io actor");
            print_exit(pi, "ping");
        }
    );
    await_all_others_done();
    shutdown();
}
