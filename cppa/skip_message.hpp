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


#ifndef CPPA_SKIP_MESSAGE_HPP
#define CPPA_SKIP_MESSAGE_HPP

namespace cppa {

/**
 * @brief Optional return type for functors used in pattern matching
 *        expressions. This type is evaluated by the runtime system of libcppa
 *        and can be used to intentionally skip messages.
 */
struct skip_message_t { constexpr skip_message_t() { } };

/**
 * @brief Tells the runtime system to skip a message when used as message
 *        handler, i.e., causes the runtime to leave the message in
 *        the mailbox of an actor.
 */
constexpr skip_message_t skip_message() {
    return {};
}

// implemented in string_serialization.cpp
std::ostream& operator<<(std::ostream&, skip_message_t);

} // namespace cppa

#endif // CPPA_SKIP_MESSAGE_HPP
