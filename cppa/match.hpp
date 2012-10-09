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


#ifndef CPPA_MATCH_HPP
#define CPPA_MATCH_HPP

#include <vector>
#include <istream>
#include <iterator>
#include <type_traits>

#include "cppa/any_tuple.hpp"
#include "cppa/partial_function.hpp"

#include "cppa/util/tbind.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa { namespace detail {

struct match_helper {
    match_helper(const match_helper&) = delete;
    match_helper& operator=(const match_helper&) = delete;
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
    bool operator()(Arg0&& arg0, Args&&... args) {
        auto tmp = match_expr_convert(std::forward<Arg0>(arg0),
                                      std::forward<Args>(args)...);
        static_assert(std::is_same<partial_function, decltype(tmp)>::value,
                      "match statement contains timeout");
        return tmp(tup);
    }
};

struct identity_fun {
    template<typename T>
    inline auto operator()(T&& arg) -> decltype(std::forward<T>(arg)) {
        return std::forward<T>(arg);
    }
};

template<typename Iterator, typename Projection = identity_fun>
class match_each_helper {

 public:

    match_each_helper(match_each_helper&&) = default;
    match_each_helper(const match_each_helper&) = delete;
    match_each_helper& operator=(const match_each_helper&) = delete;
    match_each_helper(Iterator first, Iterator last) : i(first), e(last) { }
    match_each_helper(Iterator first, Iterator last, Projection proj)
    : i(first), e(last), p(proj) { }

    template<typename... Cases>
    void operator()(match_expr<Cases...> expr) {
        for (; i != e; ++i) {
            expr(p(*i));
        }
    }

    template<class Arg0, class Arg1, class... Args>
    void operator()(Arg0&& arg0, Arg1&& arg1, Args&&... args) {
        (*this)(match_expr_collect(std::forward<Arg0>(arg0),
                                   std::forward<Arg1>(arg1),
                                   std::forward<Args>(args)...));
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

template<class Iterator, class Predicate,
         class Advance = advance_once,
         class Projection = identity_fun>
class match_for_helper {

 public:

    match_for_helper(match_for_helper&&) = default;
    match_for_helper(const match_for_helper&) = delete;
    match_for_helper& operator=(const match_for_helper&) = delete;
    match_for_helper(Iterator first, Predicate p, Advance a = Advance(), Projection pj = Projection())
    : i(first), adv(a), pred(p), proj(pj) { }

    template<typename... Cases>
    void operator()(match_expr<Cases...> expr) {
        for (; pred(i); adv(i)) {
            expr(proj(*i));
        }
    }

    template<class Arg0, class Arg1, class... Args>
    void operator()(Arg0&& arg0, Arg1&& arg1, Args&&... args) {
        (*this)(match_expr_collect(std::forward<Arg0>(arg0),
                                   std::forward<Arg1>(arg1),
                                   std::forward<Args>(args)...));
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

template<size_t N, size_t Size>
struct unwind_and_call {

    typedef unwind_and_call<N+1,Size> next;

    template<class Target, typename T, typename... Unwinded>
    static inline bool _(Target& target, std::vector<T>& vec, Unwinded&&... args) {
        return next::_(target, vec, std::forward<Unwinded>(args)..., vec[N]);
    }

    template<class Target, typename T, typename... Unwinded>
    static inline bool _(Target& target, bool& sub_result, std::vector<T>& vec, Unwinded&&... args) {
        return next::_(target, sub_result, vec, std::forward<Unwinded>(args)..., vec[N]);
    }

    template<typename T, typename InputIterator, class MatchExpr>
    static inline bool _(std::vector<T>& vec, InputIterator& pos, InputIterator end, MatchExpr& ex) {
        bool match_returned_false = false;
        if (run_case(vec, match_returned_false, pos, end, get<N>(ex.cases())) == 0) {
            if (match_returned_false) {
                return false;
            }
            return next::_(vec, pos, end, ex);
        }
        return true;
    }

};

template<size_t Size>
struct unwind_and_call<Size, Size> {

    template<class Target, typename T, typename... Unwinded>
    static inline bool _(Target& target, std::vector<T>&, Unwinded&&... args) {
        return target.first(target.second, std::forward<Unwinded>(args)...);
    }

    template<class Target, typename T, typename... Unwinded>
    static inline bool _(Target& target, bool& sub_result, std::vector<T>&, Unwinded&&... args) {
        return target.first.invoke(target.second, sub_result, std::forward<Unwinded>(args)...);
    }

    template<typename T, typename... Args>
    static inline bool _(std::vector<T>&, Args&&...) { return false; }

};

template<bool EvaluateSubResult> // default = false
struct run_case_impl {
    template<class Case, typename T>
    static inline bool _(Case& target, std::vector<T>& vec, bool&) {
        return unwind_and_call<0, Case::pattern_type::size>::_(target, vec);
    }
};

template<>
struct run_case_impl<true> {
    template<class Case, typename T>
    static inline bool _(Case& target, std::vector<T>& vec, bool& match_returned_false) {
        bool sub_result;
        if (unwind_and_call<0, Case::pattern_type::size>::_(target, sub_result, vec)) {
            if (sub_result == false) {
                match_returned_false = true;
            }
            return true;
        }
        return false;
    }
};

// Case is a projection_partial_function_pair
template<typename T, typename InputIterator, class Case>
size_t run_case(std::vector<T>& vec,
                bool& match_returned_false,
                InputIterator& pos,
                const InputIterator& end,
                Case& target) {
    static constexpr size_t num_args = Case::pattern_type::size;
    typedef typename Case::second_type partial_fun_type;
    typedef typename partial_fun_type::result_type result_type;
    typedef typename partial_fun_type::arg_types arg_types;
    typedef typename util::tl_map<arg_types, util::rm_ref>::type plain_args;
    static_assert(num_args > 0,
                  "empty match expressions are not allowed in stream matching");
    static_assert(util::tl_forall<plain_args, util::tbind<std::is_same, T>::template type>::value,
                  "match_stream<T>: at least one callback argument "
                  " is not of type T");
    while (vec.size() < num_args) {
        if (pos == end) {
            return 0;
        }
        vec.emplace_back(*pos++);
    }
    if (run_case_impl<std::is_same<result_type, bool>::value>::_(target, vec, match_returned_false)) {
        if (vec.size() == num_args) {
            vec.clear();
        }
        else {
            auto i = vec.begin();
            vec.erase(i, i + num_args);
        }
        return match_returned_false ? 0 : num_args;
    }
    return 0;
}

template<typename T, typename InputIterator>
class stream_matcher {

 public:

    typedef InputIterator iter;

    stream_matcher(iter first, iter last) : m_pos(first), m_end(last) { }

    template<typename... Cases>
    bool operator()(match_expr<Cases...>& expr) {
        while (m_pos != m_end) {
            if (!unwind_and_call<0, match_expr<Cases...>::cases_list::size>::_(m_cache, m_pos, m_end, expr)) {
                return false;
            }
        }
        return true;
    }

    template<typename... Cases>
    bool operator()(match_expr<Cases...>&& expr) {
        auto tmp = std::move(expr);
        return (*this)(tmp);
    }

    template<typename... Cases>
    bool operator()(const match_expr<Cases...>& expr) {
        auto tmp = expr;
        return (*this)(tmp);
    }

    template<typename Arg0, typename... Args>
    bool operator()(Arg0&& arg0, Args&&... args) {
        return (*this)(match_expr_collect(std::forward<Arg0>(arg0),
                                          std::forward<Args>(args)...));
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
 * @brief Starts a match expression that matches each element in
 *        range [first, last).
 * @param first Iterator to the first element.
 * @param last Iterator to the last element (excluded).
 * @returns A helper object providing <tt>operator(...)</tt>.
 */
template<typename InputIterator>
auto match_each(InputIterator first, InputIterator last)
     -> detail::match_each_helper<InputIterator> {
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
auto match_each(InputIterator first, InputIterator last, Projection proj)
     -> detail::match_each_helper<InputIterator, Projection> {
    return {first, last, std::move(proj)};
}

template<typename InputIterator, typename Predicate>
auto match_for(InputIterator first, Predicate pred)
     -> detail::match_for_helper<InputIterator, Predicate> {
    return {first, std::move(pred)};
}

template<typename InputIterator, typename Predicate, typename Advance>
auto match_for(InputIterator first, Predicate pred, Advance adv)
     -> detail::match_for_helper<InputIterator, Predicate, Advance> {
    return {first, std::move(pred), std::move(adv)};
}

template<class InputIterator, class Predicate, class Advance, class Projection>
auto match_for(InputIterator first, Predicate pred, Advance adv, Projection pj)
     -> detail::match_for_helper<InputIterator, Predicate, Advance, Projection>{
    return {first, std::move(pred), std::move(adv), std::move(pj)};
}

template<typename T>
detail::stream_matcher<T, std::istream_iterator<T> > match_stream(std::istream& stream) {
    std::istream_iterator<T> first(stream);
    std::istream_iterator<T> last; // 'end of stream' iterator
    return {first, last};
}

template<typename T, typename InputIterator>
detail::stream_matcher<T, InputIterator> match_stream(InputIterator first,
                                                      InputIterator last  ) {
    return {first, last};
}

} // namespace cppa

#endif // CPPA_MATCH_HPP
