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


#ifndef CPPA_UTIL_DURATION_HPP
#define CPPA_UTIL_DURATION_HPP

#include <string>
#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace cppa {
namespace util {

/**
 * @brief SI time units to specify timeouts.
 */
enum class time_unit : std::uint32_t {
    invalid = 0,
    seconds = 1,
    milliseconds = 1000,
    microseconds = 1000000
};

// minutes are implicitly converted to seconds
template<std::intmax_t Num, std::intmax_t Denom>
struct ratio_to_time_unit_helper {
    static constexpr time_unit value = time_unit::invalid;
};

template<> struct ratio_to_time_unit_helper<1, 1> {
    static constexpr time_unit value = time_unit::seconds;
};

template<> struct ratio_to_time_unit_helper<1, 1000> {
    static constexpr time_unit value = time_unit::milliseconds;
};

template<> struct ratio_to_time_unit_helper<1, 1000000> {
    static constexpr time_unit value = time_unit::microseconds;
};

template<> struct ratio_to_time_unit_helper<60, 1> {
    static constexpr time_unit value = time_unit::seconds;
};

/**
 * @brief Converts an STL time period to a
 *        {@link time_unit time_unit}.
 */
template<typename Period>
constexpr time_unit get_time_unit_from_period() {
    return ratio_to_time_unit_helper<Period::num, Period::den>::value;
}

/**
 * @brief Time duration consisting of a {@link time_unit time_unit}
 *        and a 64 bit unsigned integer.
 */
class duration {

 public:

    constexpr duration() : unit(time_unit::invalid), count(0) { }

    constexpr duration(time_unit u, std::uint32_t v) : unit(u), count(v) { }

    /**
     * @brief Creates a new instance from an STL duration.
     * @throws std::invalid_argument Thrown if <tt>d.count()</tt> is negative.
     */
    template<class Rep, class Period>
    duration(std::chrono::duration<Rep, Period> d)
    : unit(get_time_unit_from_period<Period>()), count(rd(d)) {
        static_assert(get_time_unit_from_period<Period>() != time_unit::invalid,
                      "cppa::duration supports only minutes, seconds, "
                      "milliseconds or microseconds");
    }

    /**
     * @brief Creates a new instance from an STL duration given in minutes.
     * @throws std::invalid_argument Thrown if <tt>d.count()</tt> is negative.
     */
    template<class Rep>
    duration(std::chrono::duration<Rep, std::ratio<60, 1> > d)
    : unit(time_unit::seconds), count(rd(d) * 60) { }

    /**
     * @brief Returns true if <tt>unit != time_unit::none</tt>.
     */
    inline bool valid() const { return unit != time_unit::invalid; }

    /**
     * @brief Returns true if <tt>count == 0</tt>.
     */
    inline bool is_zero() const { return count == 0; }

    std::string to_string() const;

    time_unit unit;

    std::uint64_t count;

 private:

    // reads d.count and throws invalid_argument if d.count < 0
    template<class Rep, class Period>
    static inline std::uint64_t
    rd(const std::chrono::duration<Rep, Period>& d) {
        if (d.count() < 0) {
            throw std::invalid_argument("negative durations are not supported");
        }
        return static_cast<std::uint64_t>(d.count());
    }

    template<class Rep>
    static inline std::uint64_t
    rd(const std::chrono::duration<Rep, std::ratio<60, 1>>& d) {
        // convert minutes to seconds on-the-fly
        return rd(std::chrono::duration<Rep, std::ratio<1, 1>>(d.count()) * 60);
    }


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

/**
 * @relates duration
 */
template<class Clock, class Duration>
std::chrono::time_point<Clock, Duration>&
operator+=(std::chrono::time_point<Clock, Duration>& lhs, const duration& rhs) {
    switch (rhs.unit) {
        case time_unit::seconds:
            lhs += std::chrono::seconds(rhs.count);
            break;

        case time_unit::milliseconds:
            lhs += std::chrono::milliseconds(rhs.count);
            break;

        case time_unit::microseconds:
            lhs += std::chrono::microseconds(rhs.count);
            break;

        case time_unit::invalid:
            break;
    }
    return lhs;
}

} // namespace util
} // namespace cppa

#endif // CPPA_UTIL_DURATION_HPP
