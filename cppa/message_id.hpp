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
 * Copyright (C) 2011-2014                                                    *
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


#ifndef CPPA_MESSAGE_ID_HPP
#define CPPA_MESSAGE_ID_HPP

#include <cstdint>

#include "cppa/config.hpp"
#include "cppa/util/comparable.hpp"

namespace cppa {

struct invalid_message_id { constexpr invalid_message_id() { } };

/**
 * @brief Denotes whether a message is asynchronous or synchronous
 * @note Asynchronous messages always have an invalid message id.
 */
class message_id : util::comparable<message_id> {

    static constexpr std::uint64_t response_flag_mask     = 0x8000000000000000;
    static constexpr std::uint64_t answered_flag_mask     = 0x4000000000000000;
    static constexpr std::uint64_t high_prioity_flag_mask = 0x2000000000000000;
    static constexpr std::uint64_t request_id_mask        = 0x1FFFFFFFFFFFFFFF;

 public:

    constexpr message_id() : m_value(0) { }

    message_id(message_id&&) = default;
    message_id(const message_id&) = default;
    message_id& operator=(message_id&&) = default;
    message_id& operator=(const message_id&) = default;

    inline message_id& operator++() {
        ++m_value;
        return *this;
    }

    inline bool is_response() const {
        return (m_value & response_flag_mask) != 0;
    }

    inline bool is_answered() const {
        return (m_value & answered_flag_mask) != 0;
    }

    inline bool is_high_priority() const {
        return (m_value & high_prioity_flag_mask) != 0;
    }

    inline bool valid() const {
        return (m_value & request_id_mask) != 0;
    }

    inline bool is_request() const {
        return valid() && !is_response();
    }

    inline message_id response_id() const {
        // the response to a response is an asynchronous message
        if (is_response()) return message_id{0};
        return message_id{valid() ? m_value | response_flag_mask : 0};
    }

    inline message_id request_id() const {
        return message_id(m_value & request_id_mask);
    }

    inline message_id with_high_priority() const {
        return message_id(m_value | high_prioity_flag_mask);
    }

    inline message_id with_normal_priority() const {
        return message_id(m_value & ~high_prioity_flag_mask);
    }

    inline void mark_as_answered() {
        m_value |= answered_flag_mask;
    }

    inline std::uint64_t integer_value() const {
        return m_value;
    }

    static inline message_id from_integer_value(std::uint64_t value) {
        message_id result;
        result.m_value = value;
        return result;
    }

    static constexpr invalid_message_id invalid = invalid_message_id{};

    constexpr message_id(invalid_message_id) : m_value(0) { }

    long compare(const message_id& other) const {
        return (m_value == other.m_value)
               ? 0 : ((m_value < other.m_value) ? -1 : 1);
    }

 private:

    explicit constexpr message_id(std::uint64_t value) : m_value(value) { }

    std::uint64_t m_value;

};

} // namespace cppa

#endif // CPPA_MESSAGE_ID_HPP
