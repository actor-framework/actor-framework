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


#ifndef CPPA_COMMON_ACTOR_OPS_HPP
#define CPPA_COMMON_ACTOR_OPS_HPP

#include "cppa/abstract_actor.hpp"

namespace cppa {

class actor;
class actor_addr;
namespace detail { class raw_access; }

/**
 * @brief Encapsulates actor operations that are valid for both {@link actor}
 *        and {@link actor_addr} handles.
 */
class common_actor_ops {

    friend class actor;
    friend class actor_addr;
    friend class detail::raw_access;

 public:

    common_actor_ops() = default;

    inline bool attach(attachable_ptr ptr);

    /**
     * @brief Convenience function that attaches the functor
     *        or function @p f to this actor.
     *
     * The actor executes <tt>f()</tt> on exit, or immediatley
     * if it already finished execution.
     * @param f A functor, function or lambda expression that's executed
     *             if the actor finishes execution.
     * @returns @c true if @p f was successfully attached to the actor;
     *          otherwise (actor already exited) @p false.
     */
    template<typename F>
    bool attach_functor(F&& f);

    inline actor_id id() const;
    
    actor_addr address() const;

    const node_id& node() const;

    /**
     * @brief Returns whether this is an address of a
     *        remote actor.
     */
    bool is_remote() const;

 private:

    common_actor_ops(abstract_actor_ptr ptr) : m_ptr(std::move(ptr)) { }

    abstract_actor_ptr m_ptr;

};

template<class F>
struct functor_attachable : attachable {
    
    F m_functor;
    
    template<typename T>
    inline functor_attachable(T&& arg) : m_functor(std::forward<T>(arg)) { }
    
    void actor_exited(std::uint32_t reason) { m_functor(reason); }
    
    bool matches(const attachable::token&) { return false; }
    
};

template<typename F>
bool common_actor_ops::attach_functor(F&& f) {
    typedef typename util::rm_const_and_ref<F>::type f_type;
    typedef functor_attachable<f_type> impl;
    return attach(attachable_ptr{new impl(std::forward<F>(f))});
}

inline actor_id common_actor_ops::id() const {
    return (m_ptr) ? m_ptr->id() : 0;
}

inline bool common_actor_ops::attach(attachable_ptr ptr) {
    return m_ptr ? m_ptr->attach(std::move(ptr)) : false;
}

} // namespace cppa

#endif // CPPA_COMMON_ACTOR_OPS_HPP
