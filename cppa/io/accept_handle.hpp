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


#ifndef CPPA_IO_ACCEPT_HANDLE_HPP
#define CPPA_IO_ACCEPT_HANDLE_HPP

#include "cppa/detail/handle.hpp"

namespace cppa {
namespace io {

class broker;

class accept_handle : public detail::handle<accept_handle> {

    friend class detail::handle<accept_handle>;

    typedef detail::handle<accept_handle> super;

 public:

    accept_handle() = default;

 private:

    inline accept_handle(int handle_id) : super{handle_id} { }

};

} // namespace io
} // namespace cppa

#endif // CPPA_IO_ACCEPT_HANDLE_HPP
