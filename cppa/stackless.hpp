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


#ifndef CPPA_STACKLESS_HPP
#define CPPA_STACKLESS_HPP

#include "cppa/detail/receive_policy.hpp"

namespace cppa {

/**
 * @brief An actor that uses the non-blocking API of @p libcppa and
 *        does not has its own stack.
 */
template<class Base, class Subtype>
class stackless : public Base {

    friend class detail::receive_policy;
    friend class detail::behavior_stack;

 protected:

    typedef stackless combined_type;

 public:

    template<typename... Ts>
    stackless(Ts&&... args) : Base(std::forward<Ts>(args)...) { }

    static constexpr auto receive_flag = detail::rp_sequential;

    bool has_behavior() {
        return this->m_bhvr_stack.empty() == false;
    }

 protected:

    void do_become(behavior&& bhvr, bool discard_old) {
        this->reset_timeout();
        this->request_timeout(bhvr.timeout());
        if (discard_old) this->m_bhvr_stack.pop_async_back();
        this->m_bhvr_stack.push_back(std::move(bhvr));
    }

    void become_waiting_for(behavior bhvr, message_id mf) {
        if (bhvr.timeout().valid()) {
            this->reset_timeout();
            this->request_timeout(bhvr.timeout());
        }
        this->m_bhvr_stack.push_back(std::move(bhvr), mf);
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

    // provoke compiler errors for usage of receive() and related functions

    /**
     * @brief Provokes a compiler error to ensure that a stackless actor
     *        does not accidently uses receive() instead of become().
     */
    template<typename... Ts>
    void receive(Ts&&...) {
        // this asssertion always fails
        static_assert((sizeof...(Ts) + 1) < 1,
                      "You shall not use receive in an event-based actor. "
                      "Use become() instead.");
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Ts>
    void receive_loop(Ts&&... args) {
        receive(std::forward<Ts>(args)...);
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Ts>
    void receive_while(Ts&&... args) {
        receive(std::forward<Ts>(args)...);
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Ts>
    void do_receive(Ts&&... args) {
        receive(std::forward<Ts>(args)...);
    }

    detail::receive_policy m_recv_policy;

};

} // namespace cppa

#endif // CPPA_STACKLESS_HPP
