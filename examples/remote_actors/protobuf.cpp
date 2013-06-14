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


#define CPPA_LOG_LEVEL 4

#include <vector>
#include <string>
#include <limits>
#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/logging.hpp"
#include "cppa/network/ipv4_acceptor.hpp"
#include "cppa/network/ipv4_io_stream.hpp"

#include "pingpong.pb.h"


#include "cppa/singletons.hpp"
#include "cppa/network/io_actor.hpp"
#include "cppa/network/middleman.hpp"
#include "cppa/network/io_service.hpp"
#include "cppa/network/io_actor_backend.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::network;

void protobuf_io(io_service* ios) {
    partial_function default_bhvr {
        on(atom("IO_closed")) >> [=] {
            cout << "IO_closed" << endl;
            self->quit(exit_reason::normal);
        },
        others() >> [=] {
            cout << "unexpected: " << to_string(self->last_dequeued()) << endl;
        }
    };
    ios->receive_policy(io_service::exactly, 4);
    become(
        partial_function{
            on(atom("IO_read"), arg_match) >> [=](const util::buffer& buf) {
                int num_bytes;
                memcpy(&num_bytes, buf.data(), 4);
                num_bytes = htonl(num_bytes);
                if (num_bytes < 0 || num_bytes > (1024 * 1024)) {
                    self->quit(exit_reason::user_defined);
                    return;
                }
                ios->receive_policy(io_service::exactly, (size_t) num_bytes);
                become(
                    partial_function{
                        on(atom("IO_read"), arg_match) >> [=](const util::buffer& buf) {
                            org::libcppa::PingOrPong p;
                            p.ParseFromArray(buf.data(), (int) buf.size());
                            auto print = [](const char* name, int value) {
                                cout << name << "{" << value << "}" << endl;
                            };
                            if (p.has_ping()) print("Ping", p.ping().id());
                            else if (p.has_pong()) print("Pong", p.pong().id());
                            else cerr << "neither Pong nor Ping!" << endl;
                            self->quit(exit_reason::normal);
                        }//,default_bhvr
                    }.or_else(default_bhvr)
                );
            }
        }.or_else(default_bhvr)
    );
}

/*
class protobuf_io : public io_actor {

    enum { await_size, await_data } state;

 public:

    protobuf_io() : state(await_size) { }

    void init() {
        io_handle().receive_policy(io_service::exactly, 4);
        become (
            on(atom("IO_read"), arg_match) >> [=](const util::buffer& buf) {
                switch (state) {
                    case await_size:
                        int num_bytes;
                        memcpy(&num_bytes, buf.data(), 4);
                        num_bytes = htonl(num_bytes);
                        if (num_bytes < 0 || num_bytes > (1024 * 1024)) {
                            quit(exit_reason::user_defined);
                            return;
                        }
                        io_handle().receive_policy(io_service::exactly,
                                                   static_cast<size_t>(num_bytes));
                        state = await_data;
                        break;
                    case await_data:
                        org::libcppa::PingOrPong p;
                        p.ParseFromArray(buf.data(), (int) buf.size());
                        auto print = [](const char* name, int value) {
                            cout << name << "{" << value << "}" << endl;
                        };
                        if (p.has_ping()) print("Ping", p.ping().id());
                        else if (p.has_pong()) print("Pong", p.pong().id());
                        else cerr << "neither Pong nor Ping!" << endl;
                        quit(exit_reason::normal);
                }
            },
            on(atom("IO_closed")) >> [=] {
                cout << "IO_closed" << endl;
                quit(exit_reason::normal);
            },
            others() >> [=] {
                cout << "unexpected: " << to_string(last_dequeued()) << endl;
            }
        );
    }

};
*/

int main(int argc, char** argv) {
    match(std::vector<string>{argv + 1, argv + argc}) (
        on("-s") >> [] {
            cout << "run in server mode" << endl;
            auto ack = network::ipv4_acceptor::create(4242);
            auto p = ack->accept_connection();
            //spawn_io<protobuf_io>(p.first, p.second);
            spawn_io(protobuf_io, std::move(p.first), std::move(p.second));
            await_all_others_done();
        },
        on_arg_match >> [](const string& host, const string& port_str) {
            auto port = static_cast<uint16_t>(stoul(port_str));
            auto serv = network::ipv4_io_stream::connect_to(host.c_str(), port);
            org::libcppa::PingOrPong p;
            p.mutable_ping()->set_id(numeric_limits<int32_t>::max());
            string buf = p.SerializeAsString();
            int32_t s = htonl(static_cast<int32_t>(buf.size()));
            serv->write(&s, 4);
            serv->write(buf.data(), buf.size());
            cout << "run in client mode" << endl;
        }
    );
}
