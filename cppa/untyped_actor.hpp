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


#ifndef CPPA_UNTYPED_ACTOR_HPP
#define CPPA_UNTYPED_ACTOR_HPP

#include "cppa/extend.hpp"
#include "cppa/stackless.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/mailbox_based.hpp"

namespace cppa {

/**
 * @extends local_actor
 */
class untyped_actor : public local_actor {

    bool has_behavior() {
        return this->m_bhvr_stack.empty() == false;
    }

    void unbecome() {
        this->m_bhvr_stack.pop_async_back();
    }

    /**
     * @brief Sets the actor's behavior and discards the previous behavior
     *        unless {@link keep_behavior} is given as first argument.
     */
    template<typename T, typename... Ts>
    inline typename std::enable_if<
        !is_behavior_policy<typename util::rm_const_and_ref<T>::type>::value,
        void
    >::type
    become(T&& arg, Ts&&... args) {
        do_become(match_expr_convert(std::forward<T>(arg),
                                     std::forward<Ts>(args)...),
                  true);
    }

    template<bool Discard, typename... Ts>
    inline void become(behavior_policy<Discard>, Ts&&... args) {
        do_become(match_expr_convert(std::forward<Ts>(args)...), Discard);
    }

    void become_waiting_for(behavior bhvr, message_id mf) {
        if (bhvr.timeout().valid()) {
            this->reset_timeout();
            this->request_timeout(bhvr.timeout());
        }
        this->m_bhvr_stack.push_back(std::move(bhvr), mf);
    }

    void do_become(behavior&& bhvr, bool discard_old) {
        this->reset_timeout();
        this->request_timeout(bhvr.timeout());
        if (discard_old) this->m_bhvr_stack.pop_async_back();
        this->m_bhvr_stack.push_back(std::move(bhvr));
    }

    inline bool has_behavior() const {
        return this->m_bhvr_stack.empty() == false;
    }

    inline behavior& get_behavior() {
        CPPA_REQUIRE(this->m_bhvr_stack.empty() == false);
        return this->m_bhvr_stack.back();
    }

    inline void handle_timeout(behavior& bhvr) {
        CPPA_REQUIRE(bhvr.timeout().valid());
        this->reset_timeout();
        bhvr.handle_timeout();
        if (this->m_bhvr_stack.empty() == false) {
            this->request_timeout(get_behavior().timeout());
        }
    }

    void exec_bhvr_stack() {
        while (!m_bhvr_stack.empty()) {
            m_bhvr_stack.exec(m_recv_policy, util::dptr<Subtype>(this));
        }
    }

    inline detail::behavior_stack& bhvr_stack() {
        return m_bhvr_stack;
    }

    inline optional<behavior&> sync_handler(message_id msg_id) {
        return m_bhvr_stack.sync_handler(msg_id);
    }

 protected:

    // allows actors to keep previous behaviors and enables unbecome()
    detail::behavior_stack m_bhvr_stack;

};

} // namespace cppa

#endif // CPPA_UNTYPED_ACTOR_HPP
