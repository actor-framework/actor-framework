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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef CPPA_UTIL_SPLIT_HPP
#define CPPA_UTIL_SPLIT_HPP

#include <cmath>       // fabs
#include <string>
#include <vector>
#include <limits>
#include <sstream>
#include <algorithm>
#include <type_traits>

#include "cppa/util/type_traits.hpp"

namespace cppa { namespace util {

std::vector<std::string> split(const std::string& str,
                               char delim = ' ',
                               bool keep_empties = true);

template<typename Iterator>
typename std::enable_if<is_forward_iterator<Iterator>::value,std::string>::type
join(Iterator begin, Iterator end, const std::string& glue = "") {
    bool first = true;
    std::ostringstream oss;
    for ( ; begin != end; ++begin) {
        if (first) first = false;
        else oss << glue;
        oss << *begin;
    }
    return oss.str();
}

template<typename Container>
typename std::enable_if<is_iterable<Container>::value,std::string>::type
join(const Container& c, const std::string& glue = "") {
    return join(c.begin(), c.end(), glue);
}

// end of recursion
inline void splice(std::string&, const std::string&) { }

template<typename T, typename... Ts>
void splice(std::string& str, const std::string& glue, T&& arg, Ts&&... args) {
    str += glue;
    str += std::forward<T>(arg);
    splice(str, glue, std::forward<Ts>(args)...);
}


/**
 * @brief Compares two values by using @p operator== unless two floating
 *        point numbers are compared. In the latter case, the function
 *        performs an epsilon comparison.
 */
template<typename T, typename U>
typename std::enable_if<
    !std::is_floating_point<T>::value && !std::is_floating_point<U>::value,
    bool
>::type
safe_equal(const T& lhs, const U& rhs) {
    return lhs == rhs;
}

template<typename T, typename U>
typename std::enable_if<
    std::is_floating_point<T>::value || std::is_floating_point<U>::value,
    bool
>::type
safe_equal(const T& lhs, const U& rhs) {
    typedef decltype(lhs - rhs) res_type;
    return std::fabs(lhs - rhs) <= std::numeric_limits<res_type>::epsilon();
}

} } // namespace cppa::util

#endif // CPPA_UTIL_SPLIT_HPP
