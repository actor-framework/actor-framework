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


#ifndef CPPA_DURATION_HPP
#define CPPA_DURATION_HPP

#include <chrono>
#include <cstdint>

namespace cppa { namespace util {

/**
 * @brief SI time units to specify timeouts.
 */
enum class time_unit : std::uint32_t {
    none = 0,
    seconds = 1,
    milliseconds = 1000,
    microseconds = 1000000
};

/**
 * @brief Converts an STL time period to a
 *        {@link cppa::util::time_unit time_unit}.
 */
template<typename Period>
constexpr time_unit get_time_unit_from_period() {
    return (Period::num != 1
            ? time_unit::none
            : (Period::den == 1
               ? time_unit::seconds
               : (Period::den == 1000
                  ? time_unit::milliseconds
                  : (Period::den == 1000000
                     ? time_unit::microseconds
                     : time_unit::none))));
}

/**
 * @brief Time duration consisting of a {@link cppa::util::time_unit time_unit}
 *        and a 32 bit unsigned integer.
 */
class duration {

 public:

    constexpr duration() : unit(time_unit::none), count(0) { }

    constexpr duration(time_unit u, std::uint32_t v) : unit(u), count(v) { }

    template<class Rep, class Period>
    constexpr duration(std::chrono::duration<Rep, Period> d)
    : unit(get_time_unit_from_period<Period>()), count(d.count()) {
        static_assert(get_time_unit_from_period<Period>() != time_unit::none,
                      "only seconds, milliseconds or microseconds allowed");
    }

    template<class Rep>
    constexpr duration(std::chrono::duration<Rep, std::ratio<60,1> > d)
    : unit(time_unit::seconds), count(d.count() * 60) { }

    /**
     * @brief Returns true if <tt>unit != time_unit::none</tt>.
     */
    inline bool valid() const { return unit != time_unit::none; }

    /**
     * @brief Returns true if <tt>count == 0</tt>.
     */
    inline bool is_zero() const { return count == 0; }

    time_unit unit;

    std::uint32_t count;

};

/**
 * @relates duration
 */
bool operator==(const duration& lhs, const duration& rhs);

/**
 * @relates duration
 */
inline bool operator!=(const duration& lhs, const duration& rhs) {
    return !(lhs == rhs);
}

} } // namespace cppa::util

/**
 * @relates cppa::util::duration
 */
template<class Clock, class Duration>
std::chrono::time_point<Clock, Duration>&
operator+=(std::chrono::time_point<Clock, Duration>& lhs,
           const cppa::util::duration& rhs) {
    switch (rhs.unit) {
        case cppa::util::time_unit::seconds:
            lhs += std::chrono::seconds(rhs.count);
            break;

        case cppa::util::time_unit::milliseconds:
            lhs += std::chrono::milliseconds(rhs.count);
            break;

        case cppa::util::time_unit::microseconds:
            lhs += std::chrono::microseconds(rhs.count);
            break;

        default: break;
    }
    return lhs;
}

#endif // CPPA_DURATION_HPP
