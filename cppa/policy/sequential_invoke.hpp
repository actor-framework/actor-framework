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


#ifndef CPPA_THREADLESS_HPP
#define CPPA_THREADLESS_HPP

#include "cppa/atom.hpp"
#include "cppa/behavior.hpp"

#include "cppa/util/duration.hpp"

#include "cppa/policy/invoke_policy.hpp"

namespace cppa { namespace policy {

/**
 * @brief An actor that is scheduled or otherwise managed.
 */
class sequential_invoke : public invoke_policy<sequential_invoke> {

 public:

    sequential_invoke() : m_has_pending_tout(false), m_pending_tout(0) { }

    static inline bool hm_should_skip(pointer node) {
        return node->marked;
    }

    template<class Actor>
    inline pointer hm_begin(Actor* self, pointer node) {
        auto previous = self->m_current_node;
        self->m_current_node = node;
        push_timeout();
        node->marked = true;
        return previous;
    }

    template<class Actor>
    inline void hm_cleanup(Actor* self, pointer previous) {
        self->m_current_node->marked = false;
        self->m_current_node = previous;
    }

    template<class Actor>
    inline void hm_revert(Actor* self, pointer previous) {
        self->m_current_node->marked = false;
        self->m_current_node = previous;
        pop_timeout();
    }

    inline void reset_timeout() {
        if (m_has_pending_tout) {
            ++m_pending_tout;
            m_has_pending_tout = false;
        }
    }

    template<class Actor>
    void request_timeout(Actor* self, const util::duration& d) {
        if (!d.valid()) m_has_pending_tout = false;
        else {
            auto msg = make_any_tuple(atom("SYNC_TOUT"), ++m_pending_tout);
            if (d.is_zero()) {
                // immediately enqueue timeout message if duration == 0s
                self->enqueue({self->address(), self}, std::move(msg));
                //auto e = this->new_mailbox_element(this, std::move(msg));
                //this->m_mailbox.enqueue(e);
            }
            else self->delayed_send_tuple(self, d, std::move(msg));
            m_has_pending_tout = true;
        }
    }

    template<class Actor>
    inline void handle_timeout(Actor*, behavior& bhvr) {
        bhvr.handle_timeout();
        reset_timeout();
    }

    inline void pop_timeout() {
        CPPA_REQUIRE(m_pending_tout > 0);
        --m_pending_tout;
    }

    inline void push_timeout() {
        ++m_pending_tout;
    }

    inline bool waits_for_timeout(std::uint32_t timeout_id) const {
        return m_has_pending_tout && m_pending_tout == timeout_id;
    }

    inline bool has_pending_timeout() const {
        return m_has_pending_tout;
    }

 private:

    bool m_has_pending_tout;
    std::uint32_t m_pending_tout;

};

} } // namespace cppa::policy

#endif // CPPA_THREADLESS_HPP
