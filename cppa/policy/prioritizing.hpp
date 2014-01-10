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


#ifndef PRIORITIZING_HPP
#define PRIORITIZING_HPP

#include <iostream>

#include "cppa/mailbox_element.hpp"
#include "cppa/message_priority.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"

namespace cppa { namespace policy {

class prioritizing {

 public:

    typedef std::vector<unique_mailbox_element_pointer> cache_type;

    typedef cache_type::iterator cache_iterator;

    template<class Actor>
    unique_mailbox_element_pointer next_message(Actor* self) {
        return unique_mailbox_element_pointer{self->mailbox().try_pop()};
    }

    template<class Actor>
    inline bool has_next_message(Actor* self) {
        return self->mailbox().can_fetch_more();
    }

    inline void push_to_cache(unique_mailbox_element_pointer ptr) {
        m_cache.push_back(std::move(ptr));
    }

    inline cache_iterator cache_begin() {
        return m_cache.begin();
    }

    inline cache_iterator cache_end() {
        return m_cache.begin();
    }

    inline void cache_erase(cache_iterator iter) {
        m_cache.erase(iter);
    }

 private:

    cache_type m_cache;

    /*
    mailbox_element* try_pop() {
        auto result = m_high_priority_mailbox.try_pop();
        return (result) ? result : this->m_mailbox.try_pop();
    }

    template<typename... Ts>
    prioritizing(Ts&&... args) : Base(std::forward<Ts>(args)...) { }

 protected:

    typedef prioritizing combined_type;

    void cleanup(std::uint32_t reason) {
        detail::sync_request_bouncer f{reason};
        m_high_priority_mailbox.close(f);
        Base::cleanup(reason);
    }

    bool mailbox_empty() {
        return    m_high_priority_mailbox.empty()
               && this->m_mailbox.empty();
    }

    void enqueue(const message_header& hdr, any_tuple msg) {
        typename Base::mailbox_type* mbox = nullptr;
        if (hdr.priority == message_priority::high) {
            mbox = &m_high_priority_mailbox;
        }
        else {
            mbox = &this->m_mailbox;
        }
        this->enqueue_impl(*mbox, hdr, std::move(msg));
    }

    typename Base::mailbox_type m_high_priority_mailbox;
    */

};

} } // namespace cppa::policy

#endif // PRIORITIZING_HPP
