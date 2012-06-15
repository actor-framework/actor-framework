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


#ifndef CPPA_EITHER_HPP
#define CPPA_EITHER_HPP

#include <new>
#include <utility>
#include <type_traits>
#include "cppa/config.hpp"

namespace cppa {

/**
 * @brief Represents either a @p Left or a @p Right.
 */
template<class Left, class Right>
class either {

    static_assert(   std::is_convertible<Left, Right>::value == false
                  && std::is_convertible<Right, Left>::value == false,
                  "Left is not allowed to be convertible to Right");

 public:

    typedef Left left_type;
    typedef Right right_type;

    /**
     * @brief Default constructor, creates a @p Left.
     */
    either() : m_is_left(true) { new (&m_left) left_type (); }

    /**
     * @brief Creates a @p Left from @p value.
     */
    either(const left_type& value) : m_is_left(true) { cr_left(value); }

    /**
     * @brief Creates a @p Left from @p value.
     */
    either(Left&& value) : m_is_left(true) { cr_left(std::move(value)); }

    /**
     * @brief Creates a @p Right from @p value.
     */
    either(const right_type& value) : m_is_left(false) { cr_right(value); }

    /**
     * @brief Creates a @p Right from @p value.
     */
    either(Right&& value) : m_is_left(false) { cr_right(std::move(value)); }

    either(const either& other) : m_is_left(other.m_is_left) {
        if (other.m_is_left) cr_left(other.m_left);
        else cr_right(other.m_right);
    }

    either(either&& other) : m_is_left(other.m_is_left) {
        if (other.m_is_left) cr_left(std::move(other.m_left));
        else cr_right(std::move(other.m_right));
    }

    ~either() { destroy(); }

    either& operator=(const either& other) {
        if (m_is_left == other.m_is_left) {
            if (m_is_left) m_left = other.m_left;
            else m_right = other.m_right;
        }
        else {
            destroy();
            m_is_left = other.m_is_left;
            if (other.m_is_left) cr_left(other.m_left);
            else cr_right(other.m_right);
        }
        return *this;
    }

    either& operator=(either&& other) {
        if (m_is_left == other.m_is_left) {
            if (m_is_left) m_left = std::move(other.m_left);
            else m_right = std::move(other.m_right);
        }
        else {
            destroy();
            m_is_left = other.m_is_left;
            if (other.m_is_left) cr_left(std::move(other.m_left));
            else cr_right(std::move(other.m_right));
        }
        return *this;
    }

    /**
     * @brief Returns @p true if this is a @p Left; otherwise @p false.
     */
    inline bool is_left() const { return m_is_left; }

    /**
     * @brief Returns @p true if this is a @p Left; otherwise @p false.
     */
    inline bool is_right() const { return !m_is_left; }

    /**
     * @brief Returns this @p either as a @p Left.
     */
    inline Left& left() {
        CPPA_REQUIRE(m_is_left);
        return m_left;
    }

    /**
     * @brief Returns this @p either as a @p Left.
     */
    inline const left_type& left() const {
        CPPA_REQUIRE(m_is_left);
        return m_left;
    }

    /**
     * @brief Returns this @p either as a @p Right.
     */
    inline Right& right() {
        CPPA_REQUIRE(!m_is_left);
        return m_right;
    }

    /**
     * @brief Returns this @p either as a @p Right.
     */
    inline const right_type& right() const {
        CPPA_REQUIRE(!m_is_left);
        return m_right;
    }

 private:

    bool m_is_left;

    union {
        left_type m_left;
        right_type m_right;
    };

    void destroy() {
        if (m_is_left) m_left.~Left();
        else m_right.~Right();
    }

    template<typename L>
    void cr_left(L&& value) {
        new (&m_left) left_type (std::forward<L>(value));
    }

    template<typename R>
    void cr_right(R&& value) {
        new (&m_right) right_type (std::forward<R>(value));
    }

};

/** @relates either */
template<typename Left, typename Right>
bool operator==(const either<Left, Right>& lhs, const either<Left, Right>& rhs) {
    if (lhs.is_left() == rhs.is_left()) {
        if (lhs.is_left()) return lhs.left() == rhs.left();
        else return lhs.right() == rhs.right();
    }
    return false;
}

/** @relates either */
template<typename Left, typename Right>
bool operator==(const either<Left, Right>& lhs, const Left& rhs) {
    return lhs.is_left() && lhs.left() == rhs;
}

/** @relates either */
template<typename Left, typename Right>
bool operator==(const Left& lhs, const either<Left, Right>& rhs) {
    return rhs == lhs;
}

/** @relates either */
template<typename Left, typename Right>
bool operator==(const either<Left, Right>& lhs, const Right& rhs) {
    return lhs.is_right() && lhs.right() == rhs;
}

/** @relates either */
template<typename Left, typename Right>
bool operator==(const Right& lhs, const either<Left, Right>& rhs) {
    return rhs == lhs;
}

/** @relates either */
template<typename Left, typename Right>
bool operator!=(const either<Left, Right>& lhs, const either<Left, Right>& rhs) {
    return !(lhs == rhs);
}

/** @relates either */
template<typename Left, typename Right>
bool operator!=(const either<Left, Right>& lhs, const Left& rhs) {
    return !(lhs == rhs);
}

/** @relates either */
template<typename Left, typename Right>
bool operator!=(const Left& lhs, const either<Left, Right>& rhs) {
    return !(rhs == lhs);
}

/** @relates either */
template<typename Left, typename Right>
bool operator!=(const either<Left, Right>& lhs, const Right& rhs) {
    return !(lhs == rhs);
}

/** @relates either */
template<typename Left, typename Right>
bool operator!=(const Right& lhs, const either<Left, Right>& rhs) {
    return !(rhs == lhs);
}

} // namespace cppa

#endif // CPPA_EITHER_HPP
