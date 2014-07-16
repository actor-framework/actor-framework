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

#ifndef CAF_DETAIL_DISABLABLE_DELETE_HPP
#define CAF_DETAIL_DISABLABLE_DELETE_HPP

namespace caf {
namespace detail {

class disablable_delete {

 public:

    constexpr disablable_delete() : m_enabled(true) {}

    inline void disable() { m_enabled = false; }

    inline void enable() { m_enabled = true; }

    template<typename T>
    inline void operator()(T* ptr) {
        if (m_enabled) delete ptr;
    }

 private:

    bool m_enabled;

};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_DISABLABLE_DELETE_HPP
