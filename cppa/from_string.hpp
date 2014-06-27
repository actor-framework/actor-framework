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


#ifndef CPPA_FROM_STRING_HPP
#define CPPA_FROM_STRING_HPP

#include <string>
#include <typeinfo>
#include <exception>

#include "cppa/uniform_type_info.hpp"

namespace cppa {

/**
 * @brief Converts a string created by {@link cppa::to_string to_string}
 *        to its original value.
 * @param what String representation of a serialized value.
 * @returns An {@link cppa::object object} instance that contains
 *          the deserialized value.
 */
uniform_value from_string(const std::string& what);

} // namespace cppa

#endif // CPPA_FROM_STRING_HPP
