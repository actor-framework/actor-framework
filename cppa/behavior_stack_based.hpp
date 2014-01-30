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


#ifndef CPPA_BEHAVIOR_STACK_BASED_HPP
#define CPPA_BEHAVIOR_STACK_BASED_HPP

#include "cppa/message_id.hpp"
#include "cppa/single_timeout.hpp"
#include "cppa/behavior_policy.hpp"

#include "cppa/detail/behavior_stack.hpp"

namespace cppa {

/**
 * @brief Mixin for actors using a stack-based message processing.
 * @note This mixin implicitly includes {@link single_timeout}.
 */
template<class Base, class Subtype>
class behavior_stack_based : public single_timeout<Base, Subtype> {

    typedef single_timeout<Base, Subtype> super;

 public:

    typedef behavior_stack_based combined_type;

    template <typename... Ts>
    behavior_stack_based(Ts&&... args) : super(std::forward<Ts>(args)...) { }

    inline void unbecome() {
        m_bhvr_stack.pop_async_back();
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

    inline bool has_behavior() const {
        return m_bhvr_stack.empty() == false;
    }

    inline behavior& get_behavior() {
        CPPA_REQUIRE(m_bhvr_stack.empty() == false);
        return m_bhvr_stack.back();
    }

    inline detail::behavior_stack& bhvr_stack() {
        return m_bhvr_stack;
    }

    optional<behavior&> sync_handler(message_id msg_id) override {
        return m_bhvr_stack.sync_handler(msg_id);
    }

    inline void remove_handler(message_id mid) {
        m_bhvr_stack.erase(mid);
    }

    void become_waiting_for(behavior bhvr, message_id mf) {
        //CPPA_LOG_TRACE(CPPA_MARG(mf, integer_value));
        if (bhvr.timeout().valid()) {
            if (bhvr.timeout().valid()) {
                this->reset_timeout();
                this->request_timeout(bhvr.timeout());
            }
            this->bhvr_stack().push_back(std::move(bhvr), mf);
        }
        this->bhvr_stack().push_back(std::move(bhvr), mf);
    }

    void do_become(behavior bhvr, bool discard_old) {
        //CPPA_LOG_TRACE(CPPA_ARG(discard_old));
        //if (discard_old) m_bhvr_stack.pop_async_back();
        //m_bhvr_stack.push_back(std::move(bhvr));
        if (discard_old) this->m_bhvr_stack.pop_async_back();
        this->reset_timeout();
        if (bhvr.timeout().valid()) {
            //CPPA_LOG_DEBUG("request timeout: " << bhvr.timeout().to_string());
            this->request_timeout(bhvr.timeout());
        }
        this->m_bhvr_stack.push_back(std::move(bhvr));
    }

 protected:

    // allows actors to keep previous behaviors and enables unbecome()
    detail::behavior_stack m_bhvr_stack;

};

} // namespace cppa

#endif // CPPA_BEHAVIOR_STACK_BASED_HPP
