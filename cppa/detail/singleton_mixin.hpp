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


#ifndef CPPA_DETAIL_SINGLETON_MIXIN_HPP
#define CPPA_DETAIL_SINGLETON_MIXIN_HPP

#include <utility>

namespace cppa {
namespace detail {

class singleton_manager;

// a mixin for simple singleton classes
template<class Derived, class Base = void>
class singleton_mixin : public Base {

    friend class singleton_manager;

    inline static Derived* create_singleton() { return new Derived; }
    inline void dispose() { delete this; }
    inline void destroy() { delete this; }
    inline void initialize() { }

 protected:

    template<typename... Ts>
    singleton_mixin(Ts&&... args) : Base(std::forward<Ts>(args)...) { }

    virtual ~singleton_mixin() { }

};

template<class Derived>
class singleton_mixin<Derived, void> {

    friend class singleton_manager;

    inline static Derived* create_singleton() { return new Derived; }
    inline void dispose() { delete this; }
    inline void destroy() { delete this; }
    inline void initialize() { }

 protected:

    virtual ~singleton_mixin() { }

};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_SINGLETON_MIXIN_HPP
