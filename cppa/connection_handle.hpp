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

#ifndef CPPA_CONNECTION_HANDLE_HPP
#define CPPA_CONNECTION_HANDLE_HPP

#include "cppa/io_handle.hpp"

namespace cppa {

/**
 * @brief Generic handle type for identifying connections.
 */
class connection_handle : public io_handle<connection_handle> {

    friend class io_handle<connection_handle>;

    typedef io_handle<connection_handle> super;

 public:

    constexpr connection_handle() {}

 private:

    inline connection_handle(int64_t handle_id) : super{handle_id} {}

};

constexpr connection_handle invalid_connection_handle = connection_handle{};

} // namespace cppa

#endif // CPPA_CONNECTION_HANDLE_HPP
