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
 * Free Software Foundation, either version 3 of the License                  *
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


#ifndef BEHAVIOR_STACK_HPP
#define BEHAVIOR_STACK_HPP

#include <vector>
#include <memory>
#include <utility>
#include <algorithm>

#include "cppa/option.hpp"
#include "cppa/config.hpp"
#include "cppa/behavior.hpp"
#include "cppa/message_id.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

namespace cppa { namespace detail {

struct behavior_stack_mover;

class behavior_stack
{

    friend struct behavior_stack_mover;

    behavior_stack(const behavior_stack&) = delete;
    behavior_stack& operator=(const behavior_stack&) = delete;

    typedef std::pair<behavior,message_id_t> element_type;

 public:

    behavior_stack() = default;

    // @pre expected_response.valid()
    option<behavior&> sync_handler(message_id_t expected_response);

    // erases the last asynchronous message handler
    void pop_async_back();

    void clear();

    // erases the synchronous response handler associated with @p rid
    void erase(message_id_t rid) {
        erase_if([=](const element_type& e) { return e.second == rid; });
    }

    inline bool empty() const { return m_elements.empty(); }

    inline behavior& back() {
        CPPA_REQUIRE(!empty());
        return m_elements.back().first;
    }

    inline void push_back(behavior&& what,
                          message_id_t response_id = message_id_t::invalid) {
        m_elements.emplace_back(std::move(what), response_id);
    }

    inline void cleanup() {
        m_erased_elements.clear();
    }

    template<class Policy, class Client>
    bool invoke(Policy& policy, Client* client, recursive_queue_node* node) {
        CPPA_REQUIRE(!empty());
        CPPA_REQUIRE(client != nullptr);
        CPPA_REQUIRE(node != nullptr);
        // use a copy of bhvr, because invoked behavior might change m_elements
        auto id = m_elements.back().second;
        auto bhvr = m_elements.back().first;
        if (policy.invoke(client, node, bhvr, id)) {
            bool repeat;
            // try to match cached messages
            do {
                // remove synchronous response handler if needed
                if (id.valid()) {
                    erase_if([id](const element_type& value) {
                        return id == value.second;
                    });
                }
                if (!empty()) {
                    id = m_elements.back().second;
                    bhvr = m_elements.back().first;
                    repeat = policy.invoke_from_cache(client, bhvr, id);
                }
                else repeat = false;
            } while (repeat);
            return true;
        }
        return false;
    }

    template<class Policy, class Client>
    void exec(Policy& policy, Client* client) {
        while (!empty()) {
            invoke(policy, client, client->receive_node());
            cleanup();
        }
    }

 private:

    std::vector<element_type> m_elements;
    std::vector<behavior> m_erased_elements;

    // note: checks wheter i points to m_elements.end() before calling erase()
    inline void erase_at(std::vector<element_type>::iterator i) {
        if (i != m_elements.end()) {
            m_erased_elements.emplace_back(std::move(i->first));
            m_elements.erase(i);
        }
    }

    inline void erase_at(std::vector<element_type>::reverse_iterator i) {
        // base iterator points to the element *after* the correct element
        if (i != m_elements.rend()) erase_at(i.base() - 1);
    }

    template<typename UnaryPredicate>
    inline void erase_if(UnaryPredicate p) {
        erase_at(std::find_if(m_elements.begin(), m_elements.end(), p));
    }

    template<typename UnaryPredicate>
    inline void rerase_if(UnaryPredicate p) {
        erase_at(std::find_if(m_elements.rbegin(), m_elements.rend(), p));
    }

};

} } // namespace cppa::detail

#endif // BEHAVIOR_STACK_HPP
