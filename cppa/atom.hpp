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

#ifndef CPPA_ATOM_HPP
#define CPPA_ATOM_HPP

#include <string>

#include "cppa/detail/atom_val.hpp"

namespace cppa {

/**
 * @brief The value type of atoms.
 */
enum class atom_value : uint64_t {
    /** @cond PRIVATE */
    dirty_little_hack = 37337
    /** @endcond */

};

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
constexpr atom_value atom(char const (&str)[Size]) {
    // last character is the NULL terminator
    static_assert(Size <= 11, "only 10 characters are allowed");
    return static_cast<atom_value>(detail::atom_val(str, 0xF));
}

} // namespace cppa

#endif // CPPA_ATOM_HPP
