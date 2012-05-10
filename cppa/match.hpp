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

struct match_helper {
    match_helper(match_helper const&) = delete;
    match_helper& operator=(match_helper const&) = delete;
    any_tuple tup;
    match_helper(any_tuple t) : tup(std::move(t)) { }
    match_helper(match_helper&&) = default;
    /*
    void operator()(partial_function&& arg) {
        partial_function tmp{std::move(arg)};
        tmp(tup);
    }
    */
    template<class Arg0, class... Args>
    void operator()(Arg0&& arg0, Args&&... args) {
        auto tmp = mexpr_concat(std::forward<Arg0>(arg0),
                                std::forward<Args>(args)...);
        tmp(tup);
    }
};

template<typename Iterator>
struct match_each_helper {
    match_each_helper(match_each_helper const&) = delete;
    match_each_helper& operator=(match_each_helper const&) = delete;
    Iterator i;
    Iterator e;
    match_each_helper(Iterator first, Iterator last) : i(first), e(last) { }
    match_each_helper(match_each_helper&&) = default;
    void operator()(partial_function&& arg) {
        partial_function tmp{std::move(arg)};
        for (; i != e; ++i) {
            tmp(any_tuple::view(*i));
        }
    }
    template<class Arg0, class... Args>
    void operator()(Arg0&& arg0, Args&&... args) { (*this)(mexpr_concat_convert(std::forward<Arg0>(arg0),
                                     std::forward<Args>(args)...));
    }
};

template<class Container>
struct copying_match_each_helper {
    copying_match_each_helper(copying_match_each_helper const&) = delete;
    copying_match_each_helper& operator=(copying_match_each_helper const&) = delete;
    Container vec;
    copying_match_each_helper(Container tmp) : vec(std::move(tmp)) { }
    copying_match_each_helper(copying_match_each_helper&&) = default;
    void operator()(partial_function&& arg) {
        partial_function tmp{std::move(arg)};
        for (auto& i : vec) {
            tmp(any_tuple::view(i));
        }
    }
    template<class Arg0, class... Args>
    void operator()(Arg0&& arg0, Args&&... args) { (*this)(mexpr_concat_convert(std::forward<Arg0>(arg0),
                                     std::forward<Args>(args)...));
    }
};

template<typename Iterator, typename Projection>
struct pmatch_each_helper {
    pmatch_each_helper(pmatch_each_helper const&) = delete;
    pmatch_each_helper& operator=(pmatch_each_helper const&) = delete;
    Iterator i;
    Iterator e;
    Projection p;
    pmatch_each_helper(pmatch_each_helper&&) = default;
    template<typename PJ>
    pmatch_each_helper(Iterator first, Iterator last, PJ&& proj)
        : i(first), e(last), p(std::forward<PJ>(proj)) {
    }
    void operator()(partial_function&& arg) {
        partial_function tmp{std::move(arg)};
        for (; i != e; ++i) {
            tmp(any_tuple::view(p(*i)));
        }
    }
    template<class Arg0, class... Args>
    void operator()(Arg0&& arg0, Args&&... args) { (*this)(mexpr_concat_convert(std::forward<Arg0>(arg0),
                                     std::forward<Args>(args)...));
    }
};

} } // namespace cppa::detail

namespace cppa {

inline detail::match_helper match(any_tuple t) {
    return std::move(t);
}

/**
 * @brief Match expression.
 */
template<typename T>
detail::match_helper match(T&& what) {
    return any_tuple::view(std::forward<T>(what));
}

/**
 * @brief Match expression that matches against all elements of @p what.
 */
template<class Container>
auto match_each(Container& what)
     -> detail::match_each_helper<decltype(std::begin(what))> {
    return {std::begin(what), std::end(what)};
}

template<typename T>
auto match_each(std::initializer_list<T> list)
    -> detail::copying_match_each_helper<std::vector<typename detail::strip_and_convert<T>::type>> {
    std::vector<typename detail::strip_and_convert<T>::type> vec;
    vec.reserve(list.size());
    for (auto& i : list) vec.emplace_back(std::move(i));
    return vec;
}

template<typename InputIterator>
auto match_each(InputIterator first, InputIterator last)
     -> detail::match_each_helper<InputIterator> {
    return {first, last};
}

template<typename InputIterator, typename Projection>
auto match_each(InputIterator first, InputIterator last, Projection proj)
     -> detail::pmatch_each_helper<InputIterator, Projection> {
    return {first, last, std::move(proj)};
}

} // namespace cppa

#endif // MATCH_HPP
