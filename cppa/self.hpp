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


#ifndef SELF_HPP
#define SELF_HPP

#include "cppa/actor.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Always points to the current actor. Similar to @c this in
 *        an object-oriented context.
 */
extern local_actor* self;

#else

class local_actor;

// convertible<...> enables "actor_ptr this_actor = self;"
class self_type : public convertible<self_type, actor*> {

    self_type(const self_type&) = delete;
    self_type& operator=(const self_type&) = delete;

 public:

    constexpr self_type() { }

    // "inherited" from convertible<...>
    inline actor* do_convert() const {
        return convert_impl();
    }

    // allow "self" wherever an local_actor or actor pointer is expected
    inline operator local_actor*() const {
        return get_impl();
    }

    inline local_actor* operator->() const {
        return get_impl();
    }

    // @pre get_unchecked() == nullptr
    inline void set(local_actor* ptr) const {
        set_impl(ptr);
    }

    // @returns The current value without converting the calling context
    //          to an actor on-the-fly.
    inline local_actor* unchecked() const {
        return get_unchecked_impl();
    }

    static void cleanup_fun(local_actor*);

 private:

    static void set_impl(local_actor*);

    static local_actor* get_unchecked_impl();

    static local_actor* get_impl();

    static actor* convert_impl();

};

/*
 * "self" emulates a new keyword. The object itself is useless, all it does
 * is to provide syntactic sugar like "self->trap_exit(true)".
 */
constexpr self_type self;

#endif

} // namespace cppa

#endif // SELF_HPP
