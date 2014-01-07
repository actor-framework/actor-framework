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


#ifndef CPPA_OPTION_HPP
#define CPPA_OPTION_HPP

#include <new>
#include <utility>

#include "cppa/none.hpp"
#include "cppa/unit.hpp"
#include "cppa/config.hpp"

namespace cppa {

/**
 * @brief Represents an optional value of @p T.
 */
template<typename T>
class optional {

 public:

    /**
     * @brief Typdef for @p T.
     */
    typedef T type;

    /* *
     * @brief Default constructor.
     * @post <tt>valid() == false</tt>
     */
    //optional() : m_valid(false) { }

    /**
     * @post <tt>valid() == false</tt>
     */
    optional(const none_t& = none) : m_valid(false) { }

    /**
     * @brief Creates an @p option from @p value.
     * @post <tt>valid() == true</tt>
     */
    optional(T value) : m_valid(false) { cr(std::move(value)); }

    optional(const optional& other) : m_valid(false) {
        if (other.m_valid) cr(other.m_value);
    }

    optional(optional&& other) : m_valid(false) {
        if (other.m_valid) cr(std::move(other.m_value));
    }

    ~optional() { destroy(); }

    optional& operator=(const optional& other) {
        if (m_valid) {
            if (other.m_valid) m_value = other.m_value;
            else destroy();
        }
        else if (other.m_valid) {
            cr(other.m_value);
        }
        return *this;
    }

    optional& operator=(optional&& other) {
        if (m_valid) {
            if (other.m_valid) m_value = std::move(other.m_value);
            else destroy();
        }
        else if (other.m_valid) {
            cr(std::move(other.m_value));
        }
        return *this;
    }

    /**
     * @brief Returns @p true if this @p option has a valid value;
     *        otherwise @p false.
     */
    inline bool valid() const { return m_valid; }

    /**
     * @brief Returns <tt>!valid()</tt>.
     */
    inline bool empty() const { return !m_valid; }

    /**
     * @copydoc valid()
     */
    inline explicit operator bool() const { return valid(); }

    /**
     * @brief Returns <tt>!valid()</tt>.
     */
    inline bool operator!() const { return empty(); }

    inline bool operator==(const none_t&) { return empty(); }

    /**
     * @brief Returns the value.
     */
    inline T& operator*() {
        CPPA_REQUIRE(valid());
        return m_value;
    }

    /**
     * @brief Returns the value.
     */
    inline const T& operator*() const {
        CPPA_REQUIRE(valid());
        return m_value;
    }

    /**
     * @brief Returns the value.
     */
    inline const T* operator->() const {
        CPPA_REQUIRE(valid());
        return &m_value;
    }

    /**
     * @brief Returns the value.
     */
    inline T* operator->() {
        CPPA_REQUIRE(valid());
        return &m_value;
    }

    /**
     * @brief Returns the value.
     */
    inline T& get() {
        CPPA_REQUIRE(valid());
        return m_value;
    }

    /**
     * @brief Returns the value.
     */
    inline const T& get() const {
        CPPA_REQUIRE(valid());
        return m_value;
    }

    /**
     * @brief Returns the value. The value is set to @p default_value
     *        if <tt>valid() == false</tt>.
     * @post <tt>valid() == true</tt>
     */
    inline const T& get_or_else(const T& default_value) const {
        if (valid()) return get();
        return default_value;
    }

 private:

    bool m_valid;
    union { T m_value; };

    void destroy() {
        if (m_valid) {
            m_value.~T();
            m_valid = false;
        }
    }

    template<typename V>
    void cr(V&& value) {
        CPPA_REQUIRE(!valid());
        m_valid = true;
        new (&m_value) T (std::forward<V>(value));
    }

};

template<typename T>
class optional<T&> {

 public:

    typedef T type;

    optional(const none_t& = none) : m_value(nullptr) { }

    optional(T& value) : m_value(&value) { }

    optional(const optional& other) = default;

    optional& operator=(const optional& other) = default;

    inline bool valid() const { return m_value != nullptr; }

    inline bool empty() const { return !valid(); }

    inline explicit operator bool() const { return valid(); }

    inline bool operator!() const { return empty(); }

    inline bool operator==(const none_t&) { return empty(); }

    inline T& operator*() {
        CPPA_REQUIRE(valid());
        return *m_value;
    }

    inline const T& operator*() const {
        CPPA_REQUIRE(valid());
        return *m_value;
    }

    inline T* operator->() {
        CPPA_REQUIRE(valid());
        return m_value;
    }

    inline const T* operator->() const {
        CPPA_REQUIRE(valid());
        return m_value;
    }

    inline T& get() {
        CPPA_REQUIRE(valid());
        return *m_value;
    }

    inline const T& get() const {
        CPPA_REQUIRE(valid());
        return *m_value;
    }

    inline const T& get_or_else(const T& default_value) const {
        if (valid()) return get();
        return default_value;
    }

 private:

    T* m_value;

};

template<>
class optional<void> {

 public:

    optional(const unit_t&) : m_valid(true) { }

    optional(const none_t& = none) : m_valid(false) { }

    inline bool valid() const { return m_valid; }

    inline bool empty() const { return !m_valid; }

    inline explicit operator bool() const { return valid(); }

    inline bool operator!() const { return empty(); }

    inline bool operator==(const none_t&) { return empty(); }

    inline const unit_t& operator*() const { return unit; }

 private:

    bool m_valid;

};

/** @relates option */
template<typename T>
struct is_optional { static constexpr bool value = false; };

template<typename T>
struct is_optional<optional<T>> { static constexpr bool value = true; };

/** @relates option */
template<typename T, typename U>
bool operator==(const optional<T>& lhs, const optional<U>& rhs) {
    if ((lhs) && (rhs)) return *lhs == *rhs;
    return false;
}

/** @relates option */
template<typename T, typename U>
bool operator==(const optional<T>& lhs, const U& rhs) {
    if (lhs) return *lhs == rhs;
    return false;
}

/** @relates option */
template<typename T, typename U>
bool operator==(const T& lhs, const optional<U>& rhs) {
    return rhs == lhs;
}

/** @relates option */
template<typename T, typename U>
bool operator!=(const optional<T>& lhs, const optional<U>& rhs) {
    return !(lhs == rhs);
}

/** @relates option */
template<typename T, typename U>
bool operator!=(const optional<T>& lhs, const U& rhs) {
    return !(lhs == rhs);
}

/** @relates option */
template<typename T, typename U>
bool operator!=(const T& lhs, const optional<U>& rhs) {
    return !(lhs == rhs);
}

} // namespace cppa

#endif // CPPA_OPTION_HPP
