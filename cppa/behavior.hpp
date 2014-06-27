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


#ifndef CPPA_BEHAVIOR_HPP
#define CPPA_BEHAVIOR_HPP

#include <functional>
#include <type_traits>

#include "cppa/match_expr.hpp"
#include "cppa/timeout_definition.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/type_traits.hpp"

namespace cppa {

class message_handler;

/**
 * @brief Describes the behavior of an actor.
 */
class behavior {

    friend class message_handler;

 public:

    typedef std::function<optional<message> (message&)> continuation_fun;

    /** @cond PRIVATE */

    typedef intrusive_ptr<detail::behavior_impl> impl_ptr;

    inline auto as_behavior_impl() const -> const impl_ptr&;

    inline behavior(impl_ptr ptr);

    /** @endcond */

    behavior() = default;
    behavior(behavior&&) = default;
    behavior(const behavior&) = default;
    behavior& operator=(behavior&&) = default;
    behavior& operator=(const behavior&) = default;

    behavior(const message_handler& fun);

    template<typename F>
    behavior(const timeout_definition<F>& arg);

    template<typename F>
    behavior(const util::duration& d, F f);

    template<typename T, typename... Ts>
    behavior(const T& arg, Ts&&... args);

    /**
     * @brief Invokes the timeout callback.
     */
    inline void handle_timeout();

    /**
     * @brief Returns the duration after which receives using
     *        this behavior should time out.
     */
    inline const util::duration& timeout() const;

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
     * @brief Adds a continuation to this behavior that is executed
     *        whenever this behavior was successfully applied to
     *        a message.
     */
    behavior add_continuation(continuation_fun fun);

    inline operator bool() const {
        return static_cast<bool>(m_impl);
    }

 private:

    impl_ptr m_impl;

};

/**
 * @brief Creates a behavior from a match expression and a timeout definition.
 * @relates behavior
 */

template<typename... Cs, typename F>
inline behavior operator,(const match_expr<Cs...>& lhs,
                          const timeout_definition<F>& rhs) {
    return {lhs, rhs};
}


/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

template<typename T, typename... Ts>
behavior::behavior(const T& arg, Ts&&... args)
: m_impl(detail::match_expr_concat(
             detail::lift_to_match_expr(arg),
             detail::lift_to_match_expr(std::forward<Ts>(args))...)) { }

template<typename F>
behavior::behavior(const timeout_definition<F>& arg)
: m_impl(detail::new_default_behavior(arg.timeout, arg.handler)) { }

template<typename F>
behavior::behavior(const util::duration& d, F f)
: m_impl(detail::new_default_behavior(d, f)) { }

inline behavior::behavior(impl_ptr ptr) : m_impl(std::move(ptr)) { }

inline void behavior::handle_timeout() {
    m_impl->handle_timeout();
}

inline const util::duration& behavior::timeout() const {
    return m_impl->timeout();
}

template<typename T>
inline optional<message> behavior::operator()(T&& arg) {
    return (m_impl) ? m_impl->invoke(std::forward<T>(arg)) : none;
}

inline auto behavior::as_behavior_impl() const -> const impl_ptr& {
    return m_impl;
}

} // namespace cppa

#endif // CPPA_BEHAVIOR_HPP
