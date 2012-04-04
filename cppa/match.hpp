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


#ifndef MATCH_HPP
#define MATCH_HPP

#include "cppa/any_tuple.hpp"
#include "cppa/partial_function.hpp"

namespace cppa { namespace detail {

struct match_helper
{
    match_helper(match_helper const&) = delete;
    match_helper& operator=(match_helper const&) = delete;
    any_tuple tup;
    match_helper(any_tuple&& t) : tup(std::move(t)) { }
    match_helper(match_helper&&) = default;
    template<class... Args>
    void operator()(partial_function&& pf, Args&&... args)
    {
        partial_function tmp;
        tmp.splice(std::move(pf), std::forward<Args>(args)...);
        tmp(std::move(tup));
    }
};

template<typename Iterator>
struct match_each_helper
{
    match_each_helper(match_each_helper const&) = delete;
    match_each_helper& operator=(match_each_helper const&) = delete;
    Iterator i;
    Iterator e;
    match_each_helper(Iterator first, Iterator last) : i(first), e(last) { }
    match_each_helper(match_each_helper&&) = default;
    template<typename... Args>
    void operator()(partial_function&& arg0, Args&&... args)
    {
        partial_function tmp;
        tmp.splice(std::move(arg0), std::forward<Args>(args)...);
        for (; i != e; ++i)
        {
            tmp(any_tuple::view(*i));
        }
    }
};

template<typename Iterator, typename Projection>
struct pmatch_each_helper
{
    pmatch_each_helper(pmatch_each_helper const&) = delete;
    pmatch_each_helper& operator=(pmatch_each_helper const&) = delete;
    Iterator i;
    Iterator e;
    Projection p;
    pmatch_each_helper(pmatch_each_helper&&) = default;
    template<typename PJ>
    pmatch_each_helper(Iterator first, Iterator last, PJ&& proj)
        : i(first), e(last), p(std::forward<PJ>(proj))
    {
    }
    template<typename... Args>
    void operator()(partial_function&& arg0, Args&&... args)
    {
        partial_function tmp;
        tmp.splice(std::move(arg0), std::forward<Args>(args)...);
        for (; i != e; ++i)
        {
            tmp(any_tuple::view(p(*i)));
        }
    }
};

} } // namespace cppa::detail

namespace cppa {

inline detail::match_helper match(any_tuple t)
{
    return std::move(t);
}

/**
 * @brief Match expression.
 */
template<typename T>
detail::match_helper match(T& what)
{
    return any_tuple::view(what);
}

/**
 * @brief Match expression that matches against all elements of @p what.
 */
template<class Container>
auto match_each(Container& what)
     -> detail::match_each_helper<decltype(std::begin(what))>
{
    return {std::begin(what), std::end(what)};
}

template<typename InputIterator>
auto match_each(InputIterator first, InputIterator last)
     -> detail::match_each_helper<InputIterator>
{
    return {first, last};
}

template<typename InputIterator, typename Projection>
auto pmatch_each(InputIterator first, InputIterator last, Projection&& proj)
     -> detail::pmatch_each_helper<InputIterator, typename util::rm_ref<Projection>::type>
{
    return {first, last, std::forward<Projection>(proj)};
}

} // namespace cppa

#endif // MATCH_HPP
