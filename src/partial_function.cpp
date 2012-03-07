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


#include "cppa/config.hpp"
#include "cppa/behavior.hpp"
#include "cppa/partial_function.hpp"
#include "cppa/detail/invokable.hpp"

namespace cppa {

partial_function::partial_function(invokable_ptr&& ptr)
{
    m_funs.push_back(std::move(ptr));
}

partial_function::partial_function(partial_function&& other)
    : m_funs(std::move(other.m_funs))
{
}

partial_function& partial_function::operator=(partial_function&& other)
{
    m_funs = std::move(other.m_funs);
    m_cache.clear();
    return *this;
}

auto partial_function::get_cache_entry(any_tuple const& value) -> cache_entry&
{
    m_dummy.first = value.type_token();
    auto end = m_cache.end();
    // note: uses >= for comparison (not a "real" upper bound)
    auto i = std::upper_bound(m_cache.begin(), end, m_dummy,
                              [](cache_element const& lhs,
                                 cache_element const& rhs)
                              {
                                  return lhs.first >= rhs.first;
                              });
    // if we didn't found a cache entry ...
    if (i == end || i->first != m_dummy.first)
    {
        // ... create one (store all invokables with matching types)
        cache_entry tmp;
        for (auto& fun : m_funs)
        {
            if (fun->types_match(value))
            {
                tmp.push_back(fun.get());
            }
        }
        // m_cache is always sorted,
        // due to emplace(upper_bound, ...) insertions
        i = m_cache.emplace(i, std::move(m_dummy.first), std::move(tmp));
    }
    return i->second;
}

void partial_function::operator()(any_tuple const& value)
{
    auto& v = get_cache_entry(value);
    (void) std::any_of(
                v.begin(), v.end(),
                [&](detail::invokable* i) { return i->unsafe_invoke(value); });
}

detail::invokable const* partial_function::definition_at(any_tuple const& value)
{
    auto& v = get_cache_entry(value);
    auto i = std::find_if(
                v.begin(), v.end(),
                [&](detail::invokable* i) { return i->could_invoke(value); });
    return (i != v.end()) ? *i : nullptr;
}

bool partial_function::defined_at(any_tuple const& value)
{
    return definition_at(value) != nullptr;
}

detail::intermediate* partial_function::get_intermediate(any_tuple const& value)
{
    detail::intermediate* result = nullptr;
    for (auto& i : get_cache_entry(value))
    {
        if ((result = i->get_unsafe_intermediate(value)) != nullptr)
        {
            return result;
        }
    }
    return nullptr;
}

behavior operator,(partial_function&& lhs, behavior&& rhs)
{
    behavior bhvr{rhs.m_timeout, std::move(rhs.m_timeout_handler)};
    bhvr.get_partial_function().splice(std::move(rhs.get_partial_function()),
                                       std::move(lhs));
    return bhvr;
}

} // namespace cppa
