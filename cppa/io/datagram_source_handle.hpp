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

#ifndef CPPA_IO_DATAGRAM_SOURCE_HANDLE_HPP
#define CPPA_IO_DATAGRAM_SOURCE_HANDLE_HPP

#include "cppa/io_handle.hpp"

namespace cppa {
namespace io {

/**
 * @brief Generic handle type for identifying connections.
 */
class datagram_source_handle : public io_handle<datagram_source_handle> {

    friend class io_handle<datagram_source_handle>;

    using super = io_handle<datagram_source_handle>;

 public:

    constexpr datagram_source_handle() { }

 private:

    inline datagram_source_handle(int64_t handle_id) : super{handle_id} { }

};

constexpr datagram_source_handle invalid_datagram_source_handle{};

} // namespace io
} // namespace cppa

#endif // CPPA_IO_DATAGRAM_SOURCE_HANDLE_HPP
