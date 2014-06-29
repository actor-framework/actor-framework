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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_MAILBOX_BASED_HPP
#define CPPA_MAILBOX_BASED_HPP

#include <type_traits>

#include "cppa/mailbox_element.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"
#include "cppa/detail/single_reader_queue.hpp"

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

    void cleanup(std::uint32_t reason) override {
        detail::sync_request_bouncer f{reason};
        m_mailbox.close(f);
        Base::cleanup(reason);
    }

 protected:

    typedef mailbox_based combined_type;

    typedef detail::single_reader_queue<mailbox_element, del> mailbox_type;

    template<typename... Ts>
    mailbox_based(Ts&&... args) : Base(std::forward<Ts>(args)...) { }

    mailbox_type m_mailbox;

};

} // namespace cppa

#endif //CPPA_MAILBOX_BASED_HPP
