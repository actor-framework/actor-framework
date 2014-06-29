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

#ifndef CPPA_DETAIL_SPLIT_HPP
#define CPPA_DETAIL_SPLIT_HPP

#include <cmath> // fabs
#include <string>
#include <vector>
#include <limits>
#include <sstream>
#include <algorithm>
#include <type_traits>

#include "cppa/detail/type_traits.hpp"

namespace cppa {
namespace detail {

/**
 * @brief Compares two values by using @p operator== unless two floating
 *        point numbers are compared. In the latter case, the function
 *        performs an epsilon comparison.
 */
template<typename T, typename U>
typename std::enable_if<   !std::is_floating_point<T>::value
                        && !std::is_floating_point<U>::value,
                        bool
>::type
safe_equal(const T& lhs, const U& rhs) {
    return lhs == rhs;
}

template<typename T, typename U>
typename std::enable_if<   std::is_floating_point<T>::value
                        || std::is_floating_point<U>::value,
                        bool
>::type
safe_equal(const T& lhs, const U& rhs) {
    typedef decltype(lhs - rhs) res_type;
    return std::fabs(lhs - rhs) <= std::numeric_limits<res_type>::epsilon();
}

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_SPLIT_HPP
