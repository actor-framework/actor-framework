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


#ifndef CPPA_IO_EVENT_HPP
#define CPPA_IO_EVENT_HPP

namespace cppa {
namespace io {

typedef int event_bitmask;

namespace event {
namespace {

constexpr event_bitmask none  = 0x00;
constexpr event_bitmask read  = 0x01;
constexpr event_bitmask write = 0x02;
constexpr event_bitmask both  = 0x03;
constexpr event_bitmask error = 0x04;

} // namespace <anonymous>
} // namespace event

template<unsigned InputEvent, unsigned OutputEvent, unsigned ErrorEvent>
inline event_bitmask from_int_bitmask(unsigned mask) {
    event_bitmask result = event::none;
    // read/write as long as possible
    if (mask & InputEvent) result = event::read;
    if (mask & OutputEvent) result |= event::write;
    if (result == event::none && mask & ErrorEvent) result = event::error;
    return result;
}

} // namespace io
} // namespace cppa

#endif // CPPA_IO_EVENT_HPP
