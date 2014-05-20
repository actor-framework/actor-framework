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
 * Copyright (C) 2011-2014                                                    *
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


#ifndef CPPA_POLICY_PRIORITIZING_HPP
#define CPPA_POLICY_PRIORITIZING_HPP

#include <list>
#include <iostream>

#include "cppa/mailbox_element.hpp"
#include "cppa/message_priority.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"

namespace cppa {
namespace policy {

class prioritizing {

 public:

    typedef std::list<unique_mailbox_element_pointer> cache_type;

    typedef cache_type::iterator cache_iterator;

    template<class Actor>
    unique_mailbox_element_pointer next_message(Actor* self) {
        if (!m_high.empty()) return take_first(m_high);
        // read whole mailbox
        unique_mailbox_element_pointer tmp{self->mailbox().try_pop()};
        while (tmp) {
            if (tmp->mid.is_high_priority()) m_high.push_back(std::move(tmp));
            else m_low.push_back(std::move(tmp));
            tmp.reset(self->mailbox().try_pop());
        }
        if (!m_high.empty()) return take_first(m_high);
        if (!m_low.empty()) return take_first(m_low);
        return unique_mailbox_element_pointer{};
    }

    template<class Actor>
    inline bool has_next_message(Actor* self) {
        return !m_high.empty() || !m_low.empty() || self->mailbox().can_fetch_more();
    }

    inline void push_to_cache(unique_mailbox_element_pointer ptr) {
        if (ptr->mid.is_high_priority()) {
            // insert before first element with low priority
            m_cache.insert(cache_low_begin(), std::move(ptr));
        }
        else m_cache.push_back(std::move(ptr));
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

    inline bool cache_empty() const {
        return m_cache.empty();
    }

    inline unique_mailbox_element_pointer cache_take_first() {
        return take_first(m_cache);
    }

    template<typename Iterator>
    inline void cache_prepend(Iterator first, Iterator last) {
        using std::make_move_iterator;
        // split input range between high and low priority messages
        cache_type high;
        cache_type low;
        for (; first != last; ++first) {
            if ((*first)->mid.is_high_priority()) high.push_back(std::move(*first));
            else low.push_back(std::move(*first));
        }
        // prepend high priority messages
        m_cache.insert(m_cache.begin(),
                       make_move_iterator(high.begin()),
                       make_move_iterator(high.end()));
        // insert low priority messages after high priority messages
        m_cache.insert(cache_low_begin(),
                       make_move_iterator(low.begin()),
                       make_move_iterator(low.end()));
    }

 private:

    cache_iterator cache_low_begin() {
        // insert before first element with low priority
        return std::find_if(m_cache.begin(), m_cache.end(),
                            [](const unique_mailbox_element_pointer& e) {
            return !e->mid.is_high_priority();
        });
    }

    inline unique_mailbox_element_pointer take_first(cache_type& from) {
        auto tmp = std::move(from.front());
        from.erase(from.begin());
        return std::move(tmp);
    }

    cache_type m_cache;
    cache_type m_high;
    cache_type m_low;

};

} // namespace policy
} // namespace cppa

#endif // CPPA_POLICY_PRIORITIZING_HPP
