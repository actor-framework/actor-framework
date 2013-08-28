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


#ifndef CPPA_MATCH_HPP
#define CPPA_MATCH_HPP

#include <vector>
#include <istream>
#include <iterator>
#include <type_traits>

#include "cppa/any_tuple.hpp"
#include "cppa/match_hint.hpp"
#include "cppa/match_expr.hpp"
#include "cppa/partial_function.hpp"

#include "cppa/util/tbind.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa { namespace detail {

class match_helper {

    match_helper(const match_helper&) = delete;
    match_helper& operator=(const match_helper&) = delete;

 public:

    match_helper(match_helper&&) = default;

    inline match_helper(any_tuple t) : tup(std::move(t)) { }

    template<typename... Ts>
    auto operator()(Ts&&... args)
    -> decltype(match_expr_collect(std::forward<Ts>(args)...)(any_tuple{})) {
        static_assert(sizeof...(Ts) > 0, "at least one argument required");
        auto tmp = match_expr_collect(std::forward<Ts>(args)...);
        return tmp(tup);
    }

 private:

    any_tuple tup;

};

struct identity_fun {
    template<typename T>
    inline auto operator()(T&& arg) -> decltype(std::forward<T>(arg)) {
        return std::forward<T>(arg);
    }
};

template<typename Iterator, typename Projection = identity_fun>
class match_each_helper {

    match_each_helper(const match_each_helper&) = delete;
    match_each_helper& operator=(const match_each_helper&) = delete;

 public:

    match_each_helper(match_each_helper&&) = default;

    match_each_helper(Iterator first, Iterator last, Projection proj = Projection{})
    : i(std::move(first)), e(std::move(last)), p(std::move(proj)) { }

    template<typename... Ts>
    Iterator operator()(Ts&&... args) {
        static_assert(sizeof...(Ts) > 0, "at least one argument required");
        auto expr = match_expr_collect(std::forward<Ts>(args)...);
        for (; i != e; ++i) {
            auto res = expr(p(*i));
            if (!res) return i;
        }
        return e;
    }

 private:

    Iterator i;
    Iterator e;
    Projection p;

};

struct advance_once {
    template<typename T>
    inline void operator()(T& what) { ++what; }
};

template<class Iterator,
         class Predicate,
         class Advance = advance_once,
         class Projection = identity_fun>
class match_for_helper {

    match_for_helper(const match_for_helper&) = delete;
    match_for_helper& operator=(const match_for_helper&) = delete;

 public:

    match_for_helper(match_for_helper&&) = default;

    match_for_helper(Iterator first, Predicate p, Advance a = Advance{}, Projection pj = Projection{})
    : i(first), adv(a), pred(p), proj(pj) { }

    template<typename... Ts>
    void operator()(Ts&&... args) {
        static_assert(sizeof...(Ts) > 0, "at least one argument required");
        auto expr = match_expr_collect(std::forward<Ts>(args)...);
        for (; pred(i); adv(i)) {
            expr(proj(*i));
        }
    }

 private:

    Iterator i;
    Advance adv;
    Predicate pred;
    Projection proj;

};

// Case is a projection_partial_function_pair
template<typename T, typename InputIterator, class Case>
size_t run_case(std::vector<T>& vec,
                bool& match_returned_false,
                InputIterator& pos,
                const InputIterator& end,
                Case& target);

template<size_t Pos, size_t Max>
struct unwind_pos_token { };

template<size_t Pos, size_t Max>
constexpr unwind_pos_token<Pos+1, Max> next(unwind_pos_token<Pos, Max>) {
    return {};
}

template<typename T>
static inline bool eval_res(const optional<T>& res, bool&) {
    return static_cast<bool>(res);
}

static inline bool eval_res(const optional<match_hint>& res, bool& skipped) {
    if (res) {
        if (*res == match_hint::skip) {
            skipped = true;
            return false;
        }
        return true;
    }
    return false;
}

template<size_t Max, class Target, typename T, typename... Ts>
bool unwind_and_call(unwind_pos_token<Max, Max>,
                     Target& target,
                     bool& skipped,
                     std::vector<T>&,
                     Ts&&... args) {
    return eval_res(target.first(target.second, std::forward<Ts>(args)...),
                    skipped);
}

template<size_t Max, typename T, typename InputIterator, class MatchExpr>
bool unwind_and_call(unwind_pos_token<Max, Max>,
                     std::vector<T>&,
                     InputIterator&,
                     InputIterator,
                     MatchExpr&) {
    return false;
}

template<size_t Pos, size_t Max, class Target, typename T, typename... Ts>
bool unwind_and_call(unwind_pos_token<Pos, Max> pt,
                     Target& target,
                     bool& skipped,
                     std::vector<T>& vec,
                     Ts&&... args) {
    return unwind_and_call(next(pt),
                           target,
                           skipped,
                           vec,
                           std::forward<Ts>(args)...,
                           vec[Pos]);
}

template<size_t Pos, size_t Max, typename T, typename InputIterator, class MatchExpr>
bool unwind_and_call(unwind_pos_token<Pos, Max> pt,
                     std::vector<T>& vec,
                     InputIterator& pos,
                     InputIterator end,
                     MatchExpr& ex) {
    bool skipped = false;
    if (run_case(vec, skipped, pos, end, get<Pos>(ex.cases())) == 0) {
        return (skipped) ? false : unwind_and_call(next(pt), vec, pos, end, ex);
    }
    return true;
}

template<class Case, typename T>
inline bool run_case_impl(Case& target, std::vector<T>& vec, bool& skipped) {
    unwind_pos_token<0, util::tl_size<typename Case::pattern_type>::value> pt;
    return unwind_and_call(pt, target, skipped, vec);
}

// Case is a projection_partial_function_pair
template<typename T, typename InputIterator, class Case>
size_t run_case(std::vector<T>& vec,
                bool& skipped,
                InputIterator& pos,
                const InputIterator& end,
                Case& target) {
    static constexpr size_t num_args = util::tl_size<typename Case::pattern_type>::value;
    typedef typename Case::second_type partial_fun_type;
    typedef typename partial_fun_type::arg_types arg_types;
    typedef typename util::tl_map<arg_types, util::rm_const_and_ref>::type plain_args;
    static_assert(num_args > 0,
                  "empty match expressions are not allowed in stream matching");
    static_assert(util::tl_forall<plain_args, util::tbind<std::is_same, T>::template type>::value,
                  "match_stream<T>: at least one callback argument "
                  "is not of type T");
    while (vec.size() < num_args) {
        if (pos == end) {
            return 0;
        }
        vec.emplace_back(*pos++);
    }
    if (run_case_impl(target, vec, skipped)) {
        if (vec.size() == num_args) {
            vec.clear();
        }
        else {
            auto i = vec.begin();
            vec.erase(i, i + num_args);
        }
        return skipped ? 0 : num_args;
    }
    return 0;
}

template<typename T, typename InputIterator>
class stream_matcher {

 public:

    typedef InputIterator iter;

    stream_matcher(iter first, iter last) : m_pos(first), m_end(last) { }

    template<typename... Ts>
    bool operator()(Ts&&... args) {
        auto expr = match_expr_collect(std::forward<Ts>(args)...);
        typedef decltype(expr) et;
        unwind_pos_token<0, util::tl_size<typename et::cases_list>::value> pt;
        while (m_pos != m_end) {
            if (!unwind_and_call(pt, m_cache, m_pos, m_end, expr)) {
                return false;
            }
        }
        return true;
    }

 private:

    iter m_pos;
    iter m_end;
    std::vector<T> m_cache;

};

} } // namespace cppa::detail

namespace cppa {

/**
 * @brief Starts a match expression.
 * @param what Tuple or value that should be matched against a pattern.
 * @returns A helper object providing <tt>operator(...)</tt>.
 */
inline detail::match_helper match(any_tuple what) {
    return std::move(what);
}

/**
 * @copydoc match(any_tuple)
 */
template<typename T>
detail::match_helper match(T&& what) {
    return any_tuple::view(std::forward<T>(what));
}

/**
 * @brief Splits @p str using @p delim and match the resulting strings.
 */
detail::match_helper
match_split(const std::string& str, char delim, bool keep_empties = false);

/**
 * @brief Starts a match expression that matches each element in
 *        range [first, last).
 * @param first Iterator to the first element.
 * @param last Iterator to the last element (excluded).
 * @returns A helper object providing <tt>operator(...)</tt>.
 */
template<typename InputIterator>
detail::match_each_helper<InputIterator>
match_each(InputIterator first, InputIterator last) {
    return {first, last};
}

/**
 * @brief Starts a match expression that matches <tt>proj(i)</tt> for
 *        each element @p i in range [first, last).
 * @param first Iterator to the first element.
 * @param last Iterator to the last element (excluded).
 * @param proj Projection or extractor functor.
 * @returns A helper object providing <tt>operator(...)</tt>.
 */
template<typename InputIterator, typename Projection>
detail::match_each_helper<InputIterator, Projection>
match_each(InputIterator first, InputIterator last, Projection proj) {
    return {first, last, std::move(proj)};
}

template<typename InputIterator, typename Predicate>
detail::match_for_helper<InputIterator, Predicate>
match_for(InputIterator first, Predicate pred) {
    return {first, std::move(pred)};
}

template<typename InputIterator, typename Predicate, typename Advance>
detail::match_for_helper<InputIterator, Predicate, Advance>
match_for(InputIterator first, Predicate pred, Advance adv) {
    return {first, std::move(pred), std::move(adv)};
}

template<class InputIterator, class Predicate, class Advance, class Projection>
detail::match_for_helper<InputIterator, Predicate, Advance, Projection>
match_for(InputIterator first, Predicate pred, Advance adv, Projection pj) {
    return {first, std::move(pred), std::move(adv), std::move(pj)};
}

template<typename T>
detail::stream_matcher<T, std::istream_iterator<T>>
match_stream(std::istream& stream) {
    std::istream_iterator<T> first(stream);
    std::istream_iterator<T> last; // 'end of stream' iterator
    return {first, last};
}

template<typename T, typename InputIterator>
detail::stream_matcher<T, InputIterator>
match_stream(InputIterator first, InputIterator last) {
    return {first, last};
}

} // namespace cppa

#endif // CPPA_MATCH_HPP
