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


#include "cppa/invoke_rules.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/detail/invokable.hpp"

namespace cppa {

util::duration timed_invoke_rules::default_timeout;
// invoke_rules_base

invoke_rules_base::invoke_rules_base(invokable_list&& ilist)
    : m_list(std::move(ilist))
{
}

invoke_rules_base::~invoke_rules_base()
{
}

bool invoke_rules_base::operator()(const any_tuple& data) const
{
    for (detail::invokable_ptr const& ptr : m_list)
    {
        if (ptr->invoke(data)) return true;
    }
    return false;
}

detail::intermediate*
invoke_rules_base::get_intermediate(const any_tuple& t) const
{
    detail::intermediate* result;
    for (detail::invokable_ptr const& ptr : m_list)
    {
        result = ptr->get_intermediate(t);
        if (result != nullptr) return result;
    }
    return nullptr;
}

// timed_invoke_rules

timed_invoke_rules::timed_invoke_rules()
{
}

timed_invoke_rules::timed_invoke_rules(timed_invoke_rules&& other)
    : super(std::move(other.m_list)), m_ti(std::move(other.m_ti))
{
}

timed_invoke_rules::timed_invoke_rules(detail::timed_invokable_ptr&& arg)
    : super(), m_ti(std::move(arg))
{
}

timed_invoke_rules::timed_invoke_rules(invokable_list&& lhs,
                                       timed_invoke_rules&& rhs)
    : super(std::move(lhs)), m_ti(std::move(rhs.m_ti))
{
    m_list.splice(m_list.begin(), rhs.m_list);
}

timed_invoke_rules& timed_invoke_rules::operator=(timed_invoke_rules&& other)
{
    m_list = std::move(other.m_list);
    other.m_list.clear();
    std::swap(m_ti, other.m_ti);
    return *this;
}

const util::duration& timed_invoke_rules::timeout() const
{
    return (m_ti != nullptr) ? m_ti->timeout() : default_timeout;
}

void timed_invoke_rules::handle_timeout() const
{
    // safe, because timed_invokable ignores the given argument
    if (m_ti != nullptr) m_ti->invoke(*static_cast<any_tuple*>(nullptr));
}

// invoke_rules

invoke_rules::invoke_rules(invokable_list&& ll) : super(std::move(ll))
{
}

invoke_rules::invoke_rules(invoke_rules&& other) : super(std::move(other.m_list))
{
}

invoke_rules::invoke_rules(std::unique_ptr<detail::invokable>&& arg)
{
    if (arg) m_list.push_back(std::move(arg));
}

invoke_rules& invoke_rules::splice(invokable_list&& ilist)
{
    m_list.splice(m_list.end(), ilist);
    return *this;
}

invoke_rules& invoke_rules::splice(invoke_rules&& other)
{
    return splice(std::move(other.m_list));
}

timed_invoke_rules invoke_rules::splice(timed_invoke_rules&& other)
{
    return timed_invoke_rules(std::move(m_list), std::move(other));
}

invoke_rules invoke_rules::operator,(invoke_rules&& other)
{
    m_list.splice(m_list.end(), other.m_list);
    return std::move(m_list);
}

timed_invoke_rules invoke_rules::operator,(timed_invoke_rules&& other)
{
    return timed_invoke_rules(std::move(m_list), std::move(other));
}

invoke_rules& invoke_rules::operator=(invoke_rules&& other)
{
    m_list = std::move(other.m_list);
    other.m_list.clear();
    return *this;
}

} // namespace cppa
