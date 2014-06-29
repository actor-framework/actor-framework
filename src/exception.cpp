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

#include <sstream>
#include <stdlib.h>

#include "cppa/config.hpp"
#include "cppa/exception.hpp"

#ifdef CPPA_WINDOWS
#include <winerror.h>
#else
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

namespace {

std::string ae_what(uint32_t reason) {
    std::ostringstream oss;
    oss << "actor exited with reason " << reason;
    return oss.str();
}

} // namespace <anonymous>

namespace cppa {

cppa_exception::~cppa_exception() {}

cppa_exception::cppa_exception(const std::string& what_str)
        : m_what(what_str) {}

cppa_exception::cppa_exception(std::string&& what_str)
        : m_what(std::move(what_str)) {}

const char* cppa_exception::what() const noexcept { return m_what.c_str(); }

actor_exited::~actor_exited() {}

actor_exited::actor_exited(uint32_t reason) : cppa_exception(ae_what(reason)) {
    m_reason = reason;
}

network_error::network_error(const std::string& str) : super(str) {}

network_error::network_error(std::string&& str) : super(std::move(str)) {}

network_error::~network_error() {}

bind_failure::bind_failure(const std::string& str) : super(str) {}

bind_failure::bind_failure(std::string&& str) : super(std::move(str)) {}

bind_failure::~bind_failure() {}

} // namespace cppa
