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


#include "cppa/config.hpp"

#include <ios> // ios_base::failure
#include <list>
#include <memory>
#include <cstring>    // memset
#include <iostream>
#include <stdexcept>

#ifndef CPPA_WINDOWS
#include <netinet/tcp.h>
#endif

#include "cppa/cppa.hpp"
#include "cppa/atom.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exception.hpp"
#include "cppa/singletons.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

#include "cppa/io/acceptor.hpp"
#include "cppa/io/protocol.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/ipv4_acceptor.hpp"
#include "cppa/io/ipv4_io_stream.hpp"

namespace cppa {

using namespace detail;
using namespace io;

namespace { protocol* proto() {
    return get_middleman()->get_protocol();
} }

void publish(actor_ptr whom, std::unique_ptr<acceptor> aptr) {
    proto()->publish(whom, move(aptr), {});
}

actor_ptr remote_actor(stream_ptr_pair io) {
    return proto()->remote_actor(io, {});
}

void publish(actor_ptr whom, std::uint16_t port, const char* addr) {
    if (!addr) proto()->publish(whom, {port});
    else proto()->publish(whom, {port, addr});
}

actor_ptr remote_actor(const char* host, std::uint16_t port) {
    return proto()->remote_actor({port, host});
}

} // namespace cppa
