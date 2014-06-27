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


#ifndef CPPA_PARTIAL_FUNCTION_HPP
#define CPPA_PARTIAL_FUNCTION_HPP

#include <list>
#include <vector>
#include <memory>
#include <utility>
#include <type_traits>

#include "cppa/on.hpp"
#include "cppa/behavior.hpp"
#include "cppa/message.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/may_have_timeout.hpp"
#include "cppa/timeout_definition.hpp"

#include "cppa/util/duration.hpp"

#include "cppa/detail/behavior_impl.hpp"

namespace cppa {

class behavior;

/**
 * @brief A partial function implementation
 *        for {@link cppa::message messages}.
 */
class message_handler {

    friend class behavior;

 public:

    /** @cond PRIVATE */

    typedef intrusive_ptr<detail::behavior_impl> impl_ptr;

    inline auto as_behavior_impl() const -> impl_ptr;

    message_handler(impl_ptr ptr);

    /** @endcond */

    message_handler() = default;
    message_handler(message_handler&&) = default;
    message_handler(const message_handler&) = default;
    message_handler& operator=(message_handler&&) = default;
    message_handler& operator=(const message_handler&) = default;

    template<typename T, typename... Ts>
    message_handler(const T& arg, Ts&&... args);

    /**
     * @brief Returns a value if @p arg was matched by one of the
     *        handler of this behavior, returns @p nothing otherwise.
     * @note This member function can return @p nothing even if
     *       {@link defined_at()} returns @p true, because {@link defined_at()}
     *       does not evaluate guards.
     */
    template<typename T>
    inline optional<message> operator()(T&& arg);

    /**
     * @brief Adds a fallback which is used where
     *        this partial function is not defined.
     */
    template<typename... Ts>
    typename std::conditional<
        util::disjunction<
            may_have_timeout<typename util::rm_const_and_ref<Ts>::type>::value...
        >::value,
        behavior,
        message_handler
    >::type
    or_else(Ts&&... args) const;

 private:

    impl_ptr m_impl;

};

template<typename... Cases>
message_handler operator,(const match_expr<Cases...>& mexpr,
                           const message_handler& pfun) {
    return mexpr.as_behavior_impl()->or_else(pfun.as_behavior_impl());
}

template<typename... Cases>
message_handler operator,(const message_handler& pfun,
                           const match_expr<Cases...>& mexpr) {
    return pfun.as_behavior_impl()->or_else(mexpr.as_behavior_impl());
}

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

template<typename T, typename... Ts>
message_handler::message_handler(const T& arg, Ts&&... args)
: m_impl(detail::match_expr_concat(
             detail::lift_to_match_expr(arg),
             detail::lift_to_match_expr(std::forward<Ts>(args))...)) { }

template<typename T>
inline optional<message> message_handler::operator()(T&& arg) {
    return (m_impl) ? m_impl->invoke(std::forward<T>(arg)) : none;
}

template<typename... Ts>
typename std::conditional<
    util::disjunction<
        may_have_timeout<typename util::rm_const_and_ref<Ts>::type>::value...
    >::value,
    behavior,
    message_handler
>::type
message_handler::or_else(Ts&&... args) const {
    // using a behavior is safe here, because we "cast"
    // it back to a partial_function when appropriate
    behavior tmp{std::forward<Ts>(args)...};
    return m_impl->or_else(tmp.as_behavior_impl());
}

inline auto message_handler::as_behavior_impl() const -> impl_ptr {
    return m_impl;
}

} // namespace cppa

#endif // CPPA_PARTIAL_FUNCTION_HPP
