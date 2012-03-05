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


#ifndef OPTION_HPP
#define OPTION_HPP

#include <new>
#include <utility>
#include "cppa/config.hpp"

namespace cppa {

/**
 * @brief Represents an optional value of @p T.
 */
template<typename T>
class option
{

 public:

    /**
     * @brief Typdef for @p T.
     */
    typedef T value_type;

    /**
     * @brief Default constructor.
     * @post <tt>valid() == false</tt>
     */
    option() : m_valid(false) { }

    /**
     * @brief Creates an @p option from @p value.
     * @post <tt>valid() == true</tt>
     */
    option(T&& value) : m_valid(false) { cr(std::move(value)); }

    /**
     * @brief Creates an @p option from @p value.
     * @post <tt>valid() == true</tt>
     */
    option(T const& value) : m_valid(false) { cr(value); }

    /**
     * @brief Copy constructor.
     */
    option(option const& other) : m_valid(false)
    {
        if (other.m_valid) cr(other.m_value);
    }

    /**
     * @brief Move constructor.
     */
    option(option&& other) : m_valid(false)
    {
        if (other.m_valid) cr(std::move(other.m_value));
    }

    ~option() { destroy(); }

    /**
     * @brief Copy assignment.
     */
    option& operator=(option const& other)
    {
        if (m_valid)
        {
            if (other.m_valid) m_value = other.m_value;
            else destroy();
        }
        else if (other.m_valid)
        {
            cr(other.m_value);
        }
        return *this;
    }

    /**
     * @brief Move assignment.
     */
    option& operator=(option&& other)
    {
        if (m_valid)
        {
            if (other.m_valid) m_value = std::move(other.m_value);
            else destroy();
        }
        else if (other.m_valid)
        {
            cr(std::move(other.m_value));
        }
        return *this;
    }

    /**
     * @brief Copy assignment.
     */
    option& operator=(T const& value)
    {
        if (m_valid) m_value = value;
        else cr(value);
        return *this;
    }

    /**
     * @brief Move assignment.
     */
    option& operator=(T& value)
    {
        if (m_valid) m_value = std::move(value);
        else cr(std::move(value));
        return *this;
    }

    /**
     * @brief Returns @p true if this @p option has a valid value;
     *        otherwise @p false.
     */
    inline bool valid() const { return m_valid; }

    /**
     * @copydoc valid()
     */
    inline explicit operator bool() const { return m_valid; }

    /**
     * @brief Returns @p false if this @p option has a valid value;
     *        otherwise @p true.
     */
    inline bool operator!() const { return !m_valid; }

    /**
     * @brief Returns the value.
     */
    inline T& operator*()
    {
        CPPA_REQUIRE(valid());
        return m_value;
    }

    /**
     * @brief Returns the value.
     */
    inline T const& operator*() const
    {
        CPPA_REQUIRE(valid());
        return m_value;
    }

    /**
     * @brief Returns the value.
     */
    inline T& get()
    {
        CPPA_REQUIRE(valid());
        return m_value;
    }

    /**
     * @brief Returns the value.
     */
    inline T const& get() const
    {
        CPPA_REQUIRE(valid());
        return m_value;
    }

    /**
     * @brief Returns the value of this @p option if it's valid
     *        or @p default_value.
     */
    inline T& get_or_else(T const& default_value)
    {
        if (!m_valid) cr(default_value);
        return m_value;
    }

    /**
     * @copydoc get_or_else(T const&)
     */
    inline T& get_or_else(T&& default_value)
    {
        if (!m_valid) cr(std::move(default_value));
        return m_value;
    }

 private:

    bool m_valid;
    union { T m_value; };

    void destroy()
    {
        if (m_valid)
        {
            m_value.~T();
            m_valid = false;
        }
    }

    template<typename V>
    void cr(V&& value)
    {
        CPPA_REQUIRE(!valid());
        m_valid = true;
        new (&m_value) T (std::forward<V>(value));
    }

};

template<typename T>
bool operator==(option<T> const& lhs, option<T> const& rhs)
{
    if ((lhs) && (rhs)) return *lhs == *rhs;
    return false;
}

template<typename T>
bool operator==(option<T> const& lhs, T const& rhs)
{
    if (lhs) return *lhs == rhs;
    return false;
}

template<typename T>
bool operator==(T const& lhs, option<T> const& rhs)
{
    return rhs == lhs;
}

template<typename T>
bool operator!=(option<T> const& lhs, option<T> const& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
bool operator!=(option<T> const& lhs, T const& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
bool operator!=(T const& lhs, option<T> const& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif // OPTION_HPP
