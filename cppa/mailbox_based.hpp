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


#ifndef CPPA_MAILBOX_BASED_HPP
#define CPPA_MAILBOX_BASED_HPP

#include <type_traits>

#include "cppa/mailbox_element.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"
#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa {

template<class Base, class Subtype>
class mailbox_based : public Base {

    typedef detail::disposer del;

 public:

    ~mailbox_based() {
        if (!m_mailbox.closed()) {
            detail::sync_request_bouncer f{this->exit_reason()};
            m_mailbox.close(f);
        }
    }

    template<typename... Ts>
    inline mailbox_element* new_mailbox_element(Ts&&... args) {
        return mailbox_element::create(std::forward<Ts>(args)...);
    }

 protected:

    typedef mailbox_based combined_type;

    typedef intrusive::single_reader_queue<mailbox_element, del> mailbox_type;

    template<typename... Ts>
    mailbox_based(Ts&&... args) : Base(std::forward<Ts>(args)...) { }

    void cleanup(std::uint32_t reason) override {
        detail::sync_request_bouncer f{reason};
        m_mailbox.close(f);
        Base::cleanup(reason);
    }

    mailbox_type m_mailbox;

};

} // namespace cppa

#endif //CPPA_MAILBOX_BASED_HPP
