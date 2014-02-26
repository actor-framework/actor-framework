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


#include <sstream>
#include "cppa/config.hpp"
#include "cppa/exception.hpp"

#ifndef CPPA_WINDOWS
#include <sys/socket.h>
#include <sys/un.h>
#else
#include <winerror.h>
#endif

#include <stdlib.h>
#include <errno.h>

namespace {

std::string ae_what(std::uint32_t reason) {
    std::ostringstream oss;
    oss << "actor exited with reason " << reason;
    return oss.str();
}

std::string be_what(int err_code) {
    switch (err_code) {
#ifndef CPPA_WINDOWS
    case EACCES: return "EACCES: address is protected; root access needed";
    case EADDRINUSE: return "EADDRINUSE: address is already in use";
    case EBADF: return "EBADF: no valid socket descriptor";
    case EINVAL: return "EINVAL: socket already bound to an address";
    case ENOTSOCK: return "ENOTSOCK: file descriptor given";
#else
    case WSAEACCES: return "EACCES: address is protected; root access needed";
    case WSAEADDRINUSE: return "EADDRINUSE: address is already in use";
    case WSAEBADF: return "EBADF: no valid socket descriptor";
    case WSAEINVAL: return "EINVAL: socket already bound to an address";
    case WSAENOTSOCK: return "ENOTSOCK: file descriptor given";
#endif
    default: break;
    }
    std::stringstream oss;
    oss << "an unknown error occurred (code: " << err_code << ")";
    return oss.str();
}

} // namespace <anonymous>


namespace cppa {

cppa_exception::cppa_exception(const std::string& what_str)
: m_what(what_str) { }

cppa_exception::cppa_exception(std::string&& what_str)
: m_what(std::move(what_str)) { }

cppa_exception::~cppa_exception() throw() { }

const char* cppa_exception::what() const throw() {
    return m_what.c_str();
}

actor_exited::actor_exited(std::uint32_t reason)
: cppa_exception(ae_what(reason)) {
    m_reason = reason;
}

network_error::network_error(const std::string& str) : cppa_exception(str) { }

network_error::network_error(std::string&& str)
: cppa_exception(std::move(str)) { }

bind_failure::bind_failure(int err_code) : network_error(be_what(err_code)) {
    m_errno = err_code;
}

} // namespace cppa
