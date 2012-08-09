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


#ifndef CPPA_MESSAGE_ID_HPP
#define CPPA_MESSAGE_ID_HPP

#include "cppa/config.hpp"

namespace cppa {

/**
 * @brief
 * @note Asynchronous messages always have an invalid message id.
 */
class message_id_t {

    static constexpr std::uint64_t response_flag_mask = 0x8000000000000000;
    static constexpr std::uint64_t answered_flag_mask = 0x4000000000000000;
    static constexpr std::uint64_t request_id_mask    = 0x3FFFFFFFFFFFFFFF;

    friend bool operator==(const message_id_t& lhs, const message_id_t& rhs);

 public:

    constexpr message_id_t() : m_value(0) { }

    message_id_t(message_id_t&&) = default;
    message_id_t(const message_id_t&) = default;
    message_id_t& operator=(message_id_t&&) = default;
    message_id_t& operator=(const message_id_t&) = default;

    inline message_id_t& operator++() {
        ++m_value;
        return *this;
    }

    inline bool is_response() const {
        return (m_value & response_flag_mask) != 0;
    }

    inline bool is_answered() const {
        return (m_value & answered_flag_mask) != 0;
    }

    inline bool valid() const {
        return m_value != 0;
    }

    inline bool is_request() const {
        return valid() && !is_response();
    }

    inline message_id_t response_id() const {
        return message_id_t(valid() ? m_value | response_flag_mask : 0);
    }

    inline message_id_t request_id() const {
        return message_id_t(m_value & request_id_mask);
    }

    inline void mark_as_answered() {
        m_value |= answered_flag_mask;
    }

    inline std::uint64_t integer_value() const {
        return m_value;
    }

    static inline message_id_t from_integer_value(std::uint64_t value) {
        message_id_t result;
        result.m_value = value;
        return result;
    }

 private:

    explicit message_id_t(std::uint64_t value) : m_value(value) { }

    std::uint64_t m_value;

};

inline bool operator==(const message_id_t& lhs, const message_id_t& rhs) {
    return lhs.m_value == rhs.m_value;
}

inline bool operator!=(const message_id_t& lhs, const message_id_t& rhs) {
    return !(lhs == rhs);
}

} // namespace cppa

#endif // CPPA_MESSAGE_ID_HPP
