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


#include "cppa/config.hpp"

#include <ios> // ios_base::failure
#include <list>
#include <memory>
#include <cstring>    // memset
#include <iostream>
#include <stdexcept>

#include <netinet/tcp.h>

#include "cppa/cppa.hpp"
#include "cppa/atom.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exception.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

#include "cppa/network/middleman.hpp"
#include "cppa/network/ipv4_acceptor.hpp"
#include "cppa/network/ipv4_io_stream.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {

using namespace detail;
using namespace network;

namespace {
protocol* proto() {
    return singleton_manager::get_middleman()->protocol(atom("DEFAULT")).get();
}
} // namespace <anonymous>

void publish(actor_ptr whom, std::unique_ptr<acceptor> aptr) {
    proto()->publish(whom, move(aptr), {});
}

actor_ptr remote_actor(io_stream_ptr_pair io) {
    return proto()->remote_actor(io, {});
}

void publish(actor_ptr whom, std::uint16_t port, const char* addr) {
    proto()->publish(whom, {port, addr});
}

actor_ptr remote_actor(const char* host, std::uint16_t port) {
    return proto()->remote_actor({port, host});
}

} // namespace cppa
