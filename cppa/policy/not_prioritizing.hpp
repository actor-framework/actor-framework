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


#ifndef NOT_PRIORITIZING_HPP
#define NOT_PRIORITIZING_HPP

#include <list>
#include <iterator>

#include "cppa/mailbox_element.hpp"

#include "cppa/policy/priority_policy.hpp"

namespace cppa {
namespace policy {

class not_prioritizing {

 public:

    typedef std::list<unique_mailbox_element_pointer> cache_type;

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
        return m_cache.end();
    }

    inline void cache_erase(cache_iterator iter) {
        m_cache.erase(iter);
    }

    inline bool cache_empty() const {
        return m_cache.empty();
    }

    inline unique_mailbox_element_pointer cache_take_first() {
        auto tmp = std::move(m_cache.front());
        m_cache.erase(m_cache.begin());
        return std::move(tmp);
    }

    template<typename Iterator>
    inline void cache_prepend(Iterator first, Iterator last) {
        auto mi = std::make_move_iterator(first);
        auto me = std::make_move_iterator(last);
        m_cache.insert(m_cache.begin(), mi, me);
    }

 private:

    cache_type m_cache;

};

} // namespace policy
} // namespace cppa

#endif // NOT_PRIORITIZING_HPP
