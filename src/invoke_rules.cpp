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
 * Copyright (C) 2011, Dominik Charousset <dominik.charousset@haw-hamburg.de> *
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

#include "cppa/invoke_rules.hpp"

namespace cppa {

invoke_rules::invoke_rules(list_type&& ll) : m_list(std::move(ll))
{
}

invoke_rules::invoke_rules(invoke_rules&& other)
    : m_list(std::move(other.m_list))
{
}

invoke_rules::invoke_rules(detail::invokable* arg)
{
    if (arg) m_list.push_back(std::unique_ptr<detail::invokable>(arg));
}

invoke_rules::invoke_rules(std::unique_ptr<detail::invokable>&& arg)
{
    if (arg) m_list.push_back(std::move(arg));
}

bool invoke_rules::operator()(const any_tuple& t) const
{
    for (auto i = m_list.begin(); i != m_list.end(); ++i)
    {
        if ((*i)->invoke(t)) return true;
    }
    return false;
}

detail::intermediate* invoke_rules::get_intermediate(const any_tuple& t) const
{
    detail::intermediate* result;
    for (auto i = m_list.begin(); i != m_list.end(); ++i)
    {
        result = (*i)->get_intermediate(t);
        if (result != nullptr) return result;
    }
    return nullptr;
}

invoke_rules& invoke_rules::splice(invoke_rules&& other)
{
    m_list.splice(m_list.end(), other.m_list);
    return *this;
}

invoke_rules invoke_rules::operator,(invoke_rules&& other)
{
    m_list.splice(m_list.end(), other.m_list);
    return std::move(m_list);
}


} // namespace cppa
