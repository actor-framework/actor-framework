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

#ifndef CAF_CONNECTION_HANDLE_HPP
#define CAF_CONNECTION_HANDLE_HPP

#include "caf/io/handle.hpp"

namespace caf {
namespace io {

/**
 * @brief Generic handle type for identifying connections.
 */
class connection_handle : public handle<connection_handle> {

    friend class handle<connection_handle>;

    using super = handle<connection_handle>;

 public:

    constexpr connection_handle() {}

 private:

    inline connection_handle(int64_t handle_id) : super{handle_id} {}

};

constexpr connection_handle invalid_connection_handle = connection_handle{};

} // namespace io
} // namespace caf

#endif // CAF_CONNECTION_HANDLE_HPP
