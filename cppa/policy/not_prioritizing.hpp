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

    inline cache_iterator cache_begin() { return m_cache.begin(); }

    inline cache_iterator cache_end() { return m_cache.end(); }

    inline void cache_erase(cache_iterator iter) { m_cache.erase(iter); }

    inline bool cache_empty() const { return m_cache.empty(); }

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
