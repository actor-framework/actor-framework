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
#include "cppa/message_future.hpp"
#include "cppa/event_based_actor.hpp"

#include "cppa/detail/typed_actor_util.hpp"

namespace cppa {

struct typed_actor_result_visitor {

    typed_actor_result_visitor() : m_hdl(self->make_response_handle()) { }

    typed_actor_result_visitor(response_handle hdl) : m_hdl(hdl) { }

    inline void operator()(const none_t&) const {
        CPPA_LOG_ERROR("a typed actor received a "
                       "non-matching message: "
                       << to_string(self->last_dequeued()));
    }

    inline void operator()() const { }

    template<typename T>
    inline void operator()(T& value) const {
        reply_to(m_hdl, std::move(value));
    }

    template<typename... Ts>
    inline void operator()(cow_tuple<Ts...>& value) const {
        reply_tuple_to(m_hdl, std::move(value));
    }

    template<typename R>
    inline void operator()(typed_continue_helper<R>& ch) const {
        auto hdl = m_hdl;
        ch.continue_with([=](R value) {
            typed_actor_result_visitor tarv{hdl};
            auto ov = make_optional_variant(std::move(value));
            apply_visitor(tarv, ov);
        });
    }

 private:

    response_handle m_hdl;

};

template<typename MatchExpr>
class typed_actor : public event_based_actor {

 public:

    typed_actor(MatchExpr expr) : m_fun(std::move(expr)) { }

 protected:

    void init() override {
        m_bhvr_stack.push_back(partial_function{
            on<anything>() >> [=] {
                auto result = m_fun(last_dequeued());
                apply_visitor(typed_actor_result_visitor{}, result);
            }
        });
    }

    void do_become(behavior&&, bool) override {
        CPPA_LOG_ERROR("typed actors are not allowed to call become()");
        quit(exit_reason::unallowed_function_call);
    }

 private:

    MatchExpr m_fun;

};

} // namespace cppa

#endif // CPPA_TYPED_ACTOR_HPP
