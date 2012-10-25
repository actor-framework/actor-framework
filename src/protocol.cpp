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


#include "cppa/network/protocol.hpp"
#include "cppa/network/middleman.hpp"

#include "cppa/detail/logging.hpp"

namespace cppa { namespace network {

protocol::protocol(abstract_middleman* parent) : m_parent(parent) { }

void protocol::run_later(std::function<void()> fun) {
    m_parent->run_later(std::move(fun));
}

void protocol::continue_reader(continuable_reader* ptr) {
    CPPA_LOG_TRACE(CPPA_ARG(ptr));
    m_parent->continue_reader(ptr);
}

void protocol::continue_writer(continuable_reader* ptr) {
    CPPA_LOG_TRACE(CPPA_ARG(ptr));
    CPPA_REQUIRE(ptr->as_writer() != nullptr);
    m_parent->continue_writer(ptr);
}

void protocol::stop_reader(continuable_reader* ptr) {
    CPPA_LOG_TRACE(CPPA_ARG(ptr));
    m_parent->stop_reader(ptr);
}

void protocol::stop_writer(continuable_reader* ptr) {
    CPPA_LOG_TRACE(CPPA_ARG(ptr));
    m_parent->stop_writer(ptr);
}

} } // namespace cppa::network
