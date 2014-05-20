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


#ifndef CPPA_SINGLETON_MIXIN_HPP
#define CPPA_SINGLETON_MIXIN_HPP

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

#endif // CPPA_SINGLETON_MIXIN_HPP
