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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_MATCH_HPP
#define CPPA_MATCH_HPP

#include <vector>
#include <istream>
#include <iterator>
#include <type_traits>

#include "cppa/message.hpp"
#include "cppa/match_expr.hpp"
#include "cppa/skip_message.hpp"
#include "cppa/message_handler.hpp"
#include "cppa/message_builder.hpp"

#include "cppa/detail/tbind.hpp"
#include "cppa/detail/type_list.hpp"

namespace cppa {
namespace detail {

class match_helper {

    match_helper(const match_helper&) = delete;
    match_helper& operator=(const match_helper&) = delete;

 public:

    match_helper(match_helper&&) = default;

    inline match_helper(message t) : tup(std::move(t)) { }

    template<typename... Ts>
    auto operator()(Ts&&... args)
    -> decltype(match_expr_collect(std::forward<Ts>(args)...)(message{})) {
        static_assert(sizeof...(Ts) > 0, "at least one argument required");
        auto tmp = match_expr_collect(std::forward<Ts>(args)...);
        return tmp(tup);
    }

 private:

    message tup;

};

template<typename T, typename InputIterator>
class stream_matcher {

 public:

    using iterator = InputIterator;

    stream_matcher(iterator first, iterator last) : m_pos(first), m_end(last) {
        // nop
    }

    template<typename... Ts>
    bool operator()(Ts&&... args) {
        auto mexpr = match_expr_collect(std::forward<Ts>(args)...);
        //TODO: static_assert -> mexpr must not have a wildcard
        constexpr size_t max_handler_args = 0; // TODO: get from mexpr
        message_handler handler = mexpr;
        while (m_pos != m_end) {
            m_mb.append(*m_pos++);
            if (m_mb.apply(handler)) {
                m_mb.clear();
            } else if (m_mb.size() == max_handler_args) {
                return false;
            }
        }
        // we have a match if all elements were consumed
        return m_mb.empty();
    }

 private:

    iterator m_pos;
    iterator m_end;
    message_builder m_mb;

};

struct identity_fun {
    template<typename T>
    inline T& operator()(T& arg) { return arg; }
};

template<typename InputIterator, typename Transformation = identity_fun>
class match_each_helper {

 public:

    using iterator = InputIterator;

    match_each_helper(iterator first, iterator last, Transformation fun)
            : m_pos(first), m_end(last), m_fun(std::move(fun)) {
        // nop
    }

    template<typename... Ts>
    bool operator()(Ts&&... args) {
        message_handler handler = match_expr_collect(std::forward<Ts>(args)...);
        for ( ; m_pos != m_end; ++m_pos) {
            if (!handler(m_fun(*m_pos))) return false;
        }
        return true;
    }

 private:

    iterator m_pos;
    iterator m_end;
    Transformation m_fun;

};

} // namespace detail
} // namespace cppa

namespace cppa {

/**
 * @brief Starts a match expression.
 * @param what Tuple that should be matched against a pattern.
 * @returns A helper object providing <tt>operator(...)</tt>.
 */
inline detail::match_helper match(message what) {
    return what;
}

/**
 * @brief Starts a match expression.
 * @param what Value that should be matched against a pattern.
 * @returns A helper object providing <tt>operator(...)</tt>.
 */
template<typename T>
detail::match_helper match(T&& what) {
    return message_builder{}.append(std::forward<T>(what)).to_message();
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
    return {first, last, detail::identity_fun{}};
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
