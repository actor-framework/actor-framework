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


#ifndef CPPA_COMPARABLE_HPP
#define CPPA_COMPARABLE_HPP

namespace cppa { namespace util {

/**
 * @brief Barton–Nackman trick implementation.
 *
 * @p Subclass must provide a @c compare member function that compares
 * to instances of @p T and returns an integer @c x with:
 * - <tt>x < 0</tt> if <tt>*this < other</tt>
 * - <tt>x > 0</tt> if <tt>*this > other</tt>
 * - <tt>x == 0</tt> if <tt>*this == other</tt>
 */
template<class Subclass, class T = Subclass>
class comparable {

    friend bool operator==(const Subclass& lhs, const T& rhs) {
        return lhs.compare(rhs) == 0;
    }

    friend bool operator==(const T& lhs, const Subclass& rhs) {
        return rhs.compare(lhs) == 0;
    }

    friend bool operator!=(const Subclass& lhs, const T& rhs) {
        return lhs.compare(rhs) != 0;
    }

    friend bool operator!=(const T& lhs, const Subclass& rhs) {
        return rhs.compare(lhs) != 0;
    }

    friend bool operator<(const Subclass& lhs, const T& rhs) {
        return lhs.compare(rhs) < 0;
    }

    friend bool operator>(const Subclass& lhs, const T& rhs) {
        return lhs.compare(rhs) > 0;
    }

    friend bool operator<(const T& lhs, const Subclass& rhs) {
        return rhs > lhs;
    }

    friend bool operator>(const T& lhs, const Subclass& rhs) {
        return rhs < lhs;
    }

    friend bool operator<=(const Subclass& lhs, const T& rhs) {
        return lhs.compare(rhs) <= 0;
    }

    friend bool operator>=(const Subclass& lhs, const T& rhs) {
        return lhs.compare(rhs) >= 0;
    }

    friend bool operator<=(const T& lhs, const Subclass& rhs) {
        return rhs >= lhs;
    }

    friend bool operator>=(const T& lhs, const Subclass& rhs) {
        return rhs <= lhs;
    }

};

template<class Subclass>
class comparable<Subclass, Subclass> {

    friend bool operator==(const Subclass& lhs, const Subclass& rhs) {
        return lhs.compare(rhs) == 0;
    }

    friend bool operator!=(const Subclass& lhs, const Subclass& rhs) {
        return lhs.compare(rhs) != 0;
    }

    friend bool operator<(const Subclass& lhs, const Subclass& rhs) {
        return lhs.compare(rhs) < 0;
    }

    friend bool operator<=(const Subclass& lhs, const Subclass& rhs) {
        return lhs.compare(rhs) <= 0;
    }

    friend bool operator>(const Subclass& lhs, const Subclass& rhs) {
        return lhs.compare(rhs) > 0;
    }

    friend bool operator>=(const Subclass& lhs, const Subclass& rhs) {
        return lhs.compare(rhs) >= 0;
    }

};

} } // cppa::util

#endif // CPPA_COMPARABLE_HPP
