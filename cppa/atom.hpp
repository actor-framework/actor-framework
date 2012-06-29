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


#ifndef CPPA_ATOM_HPP
#define CPPA_ATOM_HPP

#include <string>

#include "cppa/detail/atom_val.hpp"

namespace cppa {

/**
 * @brief The value type of atoms.
 */
enum class atom_value : std::uint64_t { dirty_little_hack = 37337 };

/**
 * @brief Returns @p what as a string representation.
 * @param what Compact representation of an atom.
 * @returns @p what as string.
 */
std::string to_string(const atom_value& what);

/**
 * @brief Creates an atom from given string literal.
 * @param str String constant representing an atom.
 * @returns A compact representation of @p str.
 */
template<size_t Size>
constexpr atom_value atom(char const (&str) [Size]) {
    // last character is the NULL terminator
    static_assert(Size <= 11, "only 10 characters are allowed");
    return static_cast<atom_value>(detail::atom_val(str, 0xF));
}

} // namespace cppa

#endif // CPPA_ATOM_HPP
