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


#ifndef CPPA_ANYTHING_HPP
#define CPPA_ANYTHING_HPP

#include <type_traits>

namespace cppa {

/**
 * @brief Acts as wildcard expression in patterns.
 */
struct anything { };

/**
 * @brief Compares two instances of {@link anything}.
 * @returns @p false
 * @relates anything
 */
inline bool operator==(const anything&, const anything&) { return true; }

/**
 * @brief Compares two instances of {@link anything}.
 * @returns @p false
 * @relates anything
 */
inline bool operator!=(const anything&, const anything&) { return false; }

/**
 * @brief Checks wheter @p T is {@link anything}.
 */
template<typename T>
struct is_anything {
    static constexpr bool value = std::is_same<T, anything>::value;
};


} // namespace cppa

#endif // CPPA_ANYTHING_HPP
