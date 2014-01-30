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


#ifndef CPPA_TYPED_BEHAVIOR_STACK_BASED_HPP
#define CPPA_TYPED_BEHAVIOR_STACK_BASED_HPP

#include <utility>

#include "cppa/single_timeout.hpp"
#include "cppa/behavior_policy.hpp"
#include "cppa/typed_event_based_actor.hpp"

#include "cppa/util/type_traits.hpp"

#include "cppa/detail/behavior_stack.hpp"

namespace cppa {

/**
 * @brief Mixin for actors using a typed stack-based message processing.
 * @note This mixin implicitly includes {@link single_timeout}.
 * @note Subtype must provide a typedef for @p behavior_type.
 */
template<class Base, typename... Signatures>
class typed_behavior_stack_based : public extend<Base>::template
                                          with<single_timeout> {

 public:

    typedef typed_behavior<Signatures...> behavior_type;

    /**
     * @brief Sets the actor's behavior and discards the previous behavior
     *        unless {@link keep_behavior} is given as first argument.
     */
    void become(behavior_type bhvr) {
        do_become(std::move(bhvr), true);
    }

    template<bool Discard>
    void become(behavior_policy<Discard>, behavior_type bhvr) {
        do_become(std::move(bhvr), Discard);
    }

    template<typename T, typename... Ts>
    inline typename std::enable_if<
        !is_behavior_policy<typename util::rm_const_and_ref<T>::type>::value,
        void
    >::type
    become(T&& arg, Ts&&... args) {
        do_become(behavior_type{std::forward<T>(arg),
                                std::forward<Ts>(args)...},
                  true);
    }

    template<bool Discard, typename... Ts>
    void become(behavior_policy<Discard>, Ts&&... args) {
        do_become(behavior_type{std::forward<Ts>(args)...}, Discard);
    }

    inline void unbecome() {
        m_bhvr_stack.pop_async_back();
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

 private:

    void do_become(behavior_type bhvr, bool discard_old) {
        if (discard_old) this->m_bhvr_stack.pop_async_back();
        this->reset_timeout();
        if (bhvr.timeout().valid()) {
            this->request_timeout(bhvr.timeout());
        }
        this->m_bhvr_stack.push_back(std::move(bhvr.unbox()));
    }

    // allows actors to keep previous behaviors and enables unbecome()
    detail::behavior_stack m_bhvr_stack;

};

} // namespace cppa

#endif // CPPA_TYPED_BEHAVIOR_STACK_BASED_HPP
