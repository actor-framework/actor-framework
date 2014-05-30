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


#ifndef CPPA_UTIL_SCOPE_GUARD_HPP
#define CPPA_UTIL_SCOPE_GUARD_HPP

#include <utility>

namespace cppa {
namespace util {

/**
 * @brief A lightweight scope guard implementation.
 */
template<typename Fun>
class scope_guard {

    scope_guard() = delete;
    scope_guard(const scope_guard&) = delete;
    scope_guard& operator=(const scope_guard&) = delete;

 public:

    scope_guard(Fun f) : m_fun(std::move(f)), m_enabled(true) { }

    scope_guard(scope_guard&& other)
    : m_fun(std::move(other.m_fun)), m_enabled(other.m_enabled) {
        other.m_enabled = false;
    }

    ~scope_guard() {
        if (m_enabled) m_fun();
    }

    /**
     * @brief Disables this guard, i.e., the guard does not
     *        run its cleanup code as it goes out of scope.
     */
    inline void disable() {
        m_enabled = false;
    }

 private:

    Fun m_fun;
    bool m_enabled;

};

/**
 * @brief Creates a guard that executes @p f as soon as it
 *        goes out of scope.
 * @relates scope_guard
 */
template<typename Fun>
scope_guard<Fun> make_scope_guard(Fun f) {
    return {std::move(f)};
}

} // namespace util
} // namespace cppa


#endif // CPPA_UTIL_SCOPE_GUARD_HPP
