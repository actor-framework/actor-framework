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


#ifndef CPPA_TYPED_ACTOR_HPP
#define CPPA_TYPED_ACTOR_HPP

#include "cppa/replies_to.hpp"
#include "cppa/typed_behavior.hpp"
#include "cppa/untyped_actor.hpp"

#include "cppa/detail/typed_actor_util.hpp"

namespace cppa {

template<typename... Signatures>
class typed_actor_ptr;

template<typename... Signatures>
class typed_actor : public untyped_actor {

 public:

    using signatures = util::type_list<Signatures...>;

    using behavior_type = typed_behavior<Signatures...>;

    using typed_pointer_type = typed_actor_ptr<Signatures...>;

 protected:

    virtual behavior_type make_behavior() = 0;

    void init() final {
        auto bhvr = make_behavior();
        m_bhvr_stack.push_back(std::move(bhvr.unbox()));
    }

    void do_become(behavior&&, bool) final {
        CPPA_LOG_ERROR("typed actors are not allowed to call become()");
        quit(exit_reason::unallowed_function_call);
    }

};

} // namespace cppa

namespace cppa { namespace detail {

template<typename... Signatures>
class default_typed_actor : public typed_actor<Signatures...> {

 public:

    template<typename... Cases>
    default_typed_actor(match_expr<Cases...> expr) : m_bhvr(std::move(expr)) { }

 protected:

    typed_behavior<Signatures...> make_behavior() override {
        return m_bhvr;
    }

 private:

    typed_behavior<Signatures...> m_bhvr;

};

} } // namespace cppa::detail

#endif // CPPA_TYPED_ACTOR_HPP
