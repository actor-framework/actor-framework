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

#ifndef CPPA_ACCEPT_HANDLE_HPP
#define CPPA_ACCEPT_HANDLE_HPP

#include "cppa/io_handle.hpp"

namespace cppa {

/**
 * @brief Generic handle type for managing incoming connections.
 */
class accept_handle : public io_handle<accept_handle> {

    friend class io_handle<accept_handle>;

    using super = io_handle<accept_handle>;

 public:

    accept_handle() = default;

 private:

    inline accept_handle(int64_t handle_id) : super{handle_id} {}

};

} // namespace cppa

#endif // CPPA_ACCEPT_HANDLE_HPP
