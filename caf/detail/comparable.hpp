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

#ifndef CAF_COMPARABLE_HPP
#define CAF_COMPARABLE_HPP

namespace caf {
namespace detail {

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

} // namespace details
} // namespace caf

#endif // CAF_COMPARABLE_HPP
