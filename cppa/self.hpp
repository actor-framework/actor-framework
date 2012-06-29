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


#ifndef CPPA_SELF_HPP
#define CPPA_SELF_HPP

#include "cppa/actor.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Always points to the current actor. Similar to @c this in
 *        an object-oriented context.
 */
extern local_actor* self;

#else // CPPA_DOCUMENTATION

class local_actor;

// convertible<...> enables "actor_ptr this_actor = self;"
class self_type : public convertible<self_type, actor*>,
                  public convertible<self_type, channel*> {

    self_type(const self_type&) = delete;
    self_type& operator=(const self_type&) = delete;

 public:

    typedef local_actor* pointer;

    constexpr self_type() { }

    // "inherited" from convertible<...>
    inline actor* do_convert() const {
        return convert_impl();
    }

    inline pointer get() const {
        return get_impl();
    }

    // allow "self" wherever an local_actor or actor pointer is expected
    inline operator pointer() const {
        return get_impl();
    }

    inline pointer operator->() const {
        return get_impl();
    }

    // @pre get_unchecked() == nullptr
    inline void set(pointer ptr) const {
        set_impl(ptr);
    }

    // @returns The current value without converting the calling context
    //          to an actor on-the-fly.
    inline pointer unchecked() const {
        return get_unchecked_impl();
    }

    inline pointer release() const {
        return release_impl();
    }

    inline void adopt(pointer ptr) const {
        adopt_impl(ptr);
    }

    static void cleanup_fun(pointer);

 private:

    static void set_impl(pointer);

    static pointer get_unchecked_impl();

    static pointer get_impl();

    static actor* convert_impl();

    static pointer release_impl();

    static void adopt_impl(pointer);

};

/*
 * "self" emulates a new keyword. The object itself is useless, all it does
 * is to provide syntactic sugar like "self->trap_exit(true)".
 */
constexpr self_type self;

class scoped_self_setter {

    scoped_self_setter(const scoped_self_setter&) = delete;
    scoped_self_setter& operator=(const scoped_self_setter&) = delete;

 public:

    inline scoped_self_setter(local_actor* new_value) {
        m_original_value = self.release();
        self.adopt(new_value);
    }

    inline ~scoped_self_setter() {
        // restore self
        static_cast<void>(self.release());
        self.adopt(m_original_value);
    }

 private:

    local_actor* m_original_value;

};

#endif // CPPA_DOCUMENTATION

} // namespace cppa

#endif // CPPA_SELF_HPP
