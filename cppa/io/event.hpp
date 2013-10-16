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


#ifndef EVENT_HPP
#define EVENT_HPP

namespace cppa { namespace io {

typedef int event_bitmask;

namespace event { namespace {

constexpr event_bitmask none  = 0x00;
constexpr event_bitmask read  = 0x01;
constexpr event_bitmask write = 0x02;
constexpr event_bitmask both  = 0x03;
constexpr event_bitmask error = 0x04;

} } // namespace <anonymous>::event

template<unsigned InputEvent, unsigned OutputEvent, unsigned ErrorEvent>
inline event_bitmask from_int_bitmask(unsigned mask) {
    event_bitmask result = event::none;
    // read/write as long as possible
    if (mask & InputEvent) result = event::read;
    if (mask & OutputEvent) result |= event::write;
    if (result == event::none && mask & ErrorEvent) result = event::error;
    return result;
}

} } // namespace cppa::io

#endif // EVENT_HPP
