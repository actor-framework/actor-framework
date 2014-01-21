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

#include "cppa/detail/behavior_stack.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Policy tag that causes {@link untyped_actor::become} to
 *        discard the current behavior.
 * @relates local_actor
 */
constexpr auto discard_behavior;

/**
 * @brief Policy tag that causes {@link untyped_actor::become} to
 *        keep the current behavior available.
 * @relates local_actor
 */
constexpr auto keep_behavior;

#else

template<bool DiscardBehavior>
struct behavior_policy { static constexpr bool discard_old = DiscardBehavior; };

template<typename T>
struct is_behavior_policy : std::false_type { };

template<bool DiscardBehavior>
struct is_behavior_policy<behavior_policy<DiscardBehavior>> : std::true_type { };

typedef behavior_policy<false> keep_behavior_t;
typedef behavior_policy<true > discard_behavior_t;

constexpr discard_behavior_t discard_behavior = discard_behavior_t{};

constexpr keep_behavior_t keep_behavior = keep_behavior_t{};

#endif

template<class Base, class Subtype>
class behavior_stack_based : public Base {

    typedef Base super;

 public:

    typedef behavior_stack_based combined_type;

    template <typename... Ts>
    behavior_stack_based(Ts&&... args)
        : super(std::forward<Ts>(args)...), m_has_timeout(false)
        , m_timeout_id(0) { }

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

    inline optional<behavior&> sync_handler(message_id msg_id) {
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

    // timeout handling

    void request_timeout(const util::duration& d) {
        if (d.valid()) {
            m_has_timeout = true;
            auto tid = ++m_timeout_id;
            auto msg = make_any_tuple(atom("SYNC_TOUT"), tid);
            if (d.is_zero()) {
                // immediately enqueue timeout message if duration == 0s
                this->enqueue({this->address(), this}, std::move(msg));
                //auto e = this->new_mailbox_element(this, std::move(msg));
                //this->m_mailbox.enqueue(e);
            }
            else this->delayed_send_tuple(this, d, std::move(msg));
        }
        else m_has_timeout = false;
    }

    inline bool waits_for_timeout(std::uint32_t timeout_id) const {
        return m_has_timeout && m_timeout_id == timeout_id;
    }

    inline bool is_active_timeout(std::uint32_t tid) const {
        return waits_for_timeout(tid);
    }

    inline void reset_timeout() {
        m_has_timeout = false;
    }

    inline void handle_timeout(behavior& bhvr, std::uint32_t timeout_id) {
        CPPA_REQUIRE(m_timeout_id == timeout_id);
        m_has_timeout = false;
        bhvr.handle_timeout();
    }

 protected:

    // allows actors to keep previous behaviors and enables unbecome()
    detail::behavior_stack m_bhvr_stack;

    bool m_has_timeout;
    std::uint32_t m_timeout_id;

};

} // namespace cppa

#endif // CPPA_BEHAVIOR_STACK_BASED_HPP
