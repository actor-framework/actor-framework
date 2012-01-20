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

namespace cppa {

template<typename T>
class option
{

    union
    {
        T m_value;
    };

    bool m_valid;

    void destroy()
    {
        if (m_valid) m_value.~T();
    }

    template<typename V>
    void cr(V&& value)
    {
        m_valid = true;
        new (&m_value) T (std::forward<V>(value));
    }

 public:

    typedef T value_type;

    option() : m_valid(false) { }

    template<typename V>
    option(V&& value)
    {
        cr(std::forward<V>(value));
    }

    option(option const& other)
    {
        if (other.m_valid) cr(other.m_value);
        else m_valid = false;
    }

    option(option&& other)
    {
        if (other.m_valid) cr(std::move(other.m_value));
        else m_valid = false;
    }

    ~option()
    {
        destroy();
    }

    option& operator=(option const& other)
    {
        if (m_valid)
        {
            if (other.m_valid)
            {
                m_value = other.m_value;
            }
            else
            {
                destroy();
                m_valid = false;
            }
        }
        else
        {
            if (other.m_valid)
            {
                cr(other.m_value);
            }
        }
        return *this;
    }

    option& operator=(option&& other)
    {
        if (m_valid)
        {
            if (other.m_valid)
            {
                m_value = std::move(other.m_value);
            }
            else
            {
                destroy();
                m_valid = false;
            }
        }
        else
        {
            if (other.m_valid)
            {
                cr(std::move(other.m_value));
            }
        }
        return *this;
    }

    option& operator=(T const& value)
    {
        if (m_valid) m_value = value;
        else cr(value);
        return *this;
    }

    option& operator=(T& value)
    {
        if (m_valid) m_value = std::move(value);
        else cr(std::move(value));
        return *this;
    }

    inline bool valid() const { return m_valid; }

    inline explicit operator bool() const { return m_valid; }

    inline bool operator!() const { return !m_valid; }

    inline T& operator*() { return m_value; }
    inline T const& operator*() const { return m_value; }

    inline T& get() { return m_value; }

    inline T const& get() const { return m_value; }

    inline T& get_or_else(T const& val)
    {
        if (!m_valid) cr(val);
        return m_value;
    }

    inline T& get_or_else(T&& val)
    {
        if (!m_valid) cr(std::move(val));
        return m_value;
    }

};

} // namespace cppa

#endif // OPTION_HPP
