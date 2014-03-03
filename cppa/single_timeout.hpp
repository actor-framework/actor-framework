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


#ifndef CPPA_SINGLE_TIMEOUT_HPP
#define CPPA_SINGLE_TIMEOUT_HPP

#include "cppa/any_tuple.hpp"
#include "cppa/system_messages.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {

/**
 * @brief Mixin for actors using a non-nestable message processing.
 */
template<class Base, class Subtype>
class single_timeout : public Base {

    typedef Base super;

 public:

    typedef single_timeout combined_type;

    template <typename... Ts>
    single_timeout(Ts&&... args)
            : super(std::forward<Ts>(args)...), m_has_timeout(false)
            , m_timeout_id(0) { }

    void request_timeout(const util::duration& d) {
        if (d.valid()) {
            m_has_timeout = true;
            auto tid = ++m_timeout_id;
            auto msg = make_any_tuple(timeout_msg{tid});
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

    inline bool has_active_timeout() const {
        return m_has_timeout;
    }

    inline void reset_timeout() {
        m_has_timeout = false;
    }

 protected:

    bool m_has_timeout;
    std::uint32_t m_timeout_id;

};

} // namespace cppa

#endif // CPPA_SINGLE_TIMEOUT_HPP
