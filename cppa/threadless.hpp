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

#include "cppa/send.hpp"

namespace cppa {

/**
 * @brief An actor that is scheduled or otherwise managed.
 */
template<class Base, class Subtype>
class threadless : public Base {

 protected:

    typedef threadless combined_type;

 public:

    static constexpr bool has_blocking_receive = false;

    template<typename... Ts>
    threadless(Ts&&... args) : Base(std::forward<Ts>(args)...)
                             , m_has_pending_tout(false)
                             , m_pending_tout(0) { }

    inline void reset_timeout() {
        if (m_has_pending_tout) {
            ++m_pending_tout;
            m_has_pending_tout = false;
        }
    }

    void request_timeout(const util::duration& d) {
        if (!d.valid()) m_has_pending_tout = false;
        else {
            auto msg = make_any_tuple(atom("SYNC_TOUT"), ++m_pending_tout);
            if (d.is_zero()) {
                // immediately enqueue timeout message if duration == 0s
                this->enqueue(this, std::move(msg));
                //auto e = this->new_mailbox_element(this, std::move(msg));
                //this->m_mailbox.enqueue(e);
            }
            else delayed_send_tuple(this, d, std::move(msg));
            m_has_pending_tout = true;
        }
    }

    inline void handle_timeout(behavior& bhvr) {
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

} // namespace cppa

#endif // CPPA_THREADLESS_HPP
