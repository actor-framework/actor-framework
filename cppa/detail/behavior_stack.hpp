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
 * Copyright (C) 2011, 2012                                                   *
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

#include "cppa/config.hpp"
#include "cppa/behavior.hpp"
#include "cppa/message_id.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

namespace cppa { namespace detail {

class behavior_stack_help_iterator;

class behavior_stack
{

    friend class behavior_stack_help_iterator;

    behavior_stack(const behavior_stack&) = delete;
    behavior_stack& operator=(const behavior_stack&) = delete;

    typedef std::pair<behavior, message_id_t> element_type;

 public:

    behavior_stack() = default;

    inline bool empty() const { return m_elements.empty(); }

    inline behavior& back() {
        CPPA_REQUIRE(!empty());
        return m_elements.back().first;
    }

    // erases the last asynchronous message handler
    void pop_async_back();

    void push_back(behavior&& what,
                   message_id_t expected_response = message_id_t());

    void cleanup();

    void clear();

    template<class Policy, class Client>
    bool invoke(Policy& policy, Client* client, recursive_queue_node* node) {
        CPPA_REQUIRE(!m_elements.empty());
        CPPA_REQUIRE(client != nullptr);
        CPPA_REQUIRE(node != nullptr);
        auto id = m_elements.back().second;
        if (policy.invoke(client, node, back(), id)) {
            // try to match cached messages
            do {
                // remove synchronous response handler if needed
                if (id.valid()) {
                    auto last = m_elements.end();
                    auto i = std::find_if(m_elements.begin(), last,
                                          [id](element_type& e) {
                                              return id == e.second;
                                          });
                    if (i != last) {
                        m_erased_elements.emplace_back(std::move(i->first));
                        m_elements.erase(i);
                    }
                }
                cleanup();
                id = empty() ? message_id_t() : m_elements.back().second;
            } while (!empty() && policy.invoke_from_cache(client, back(), id));
            return true;
        }
        return false;
    }

    template<class Policy, class Client>
    void exec(Policy& policy, Client* client) {
        while (!empty()) {
            invoke(policy, client, client->receive_node());
        }
    }

 private:

    std::vector<element_type> m_elements;
    std::vector<behavior> m_erased_elements;

};

} } // namespace cppa::detail

#endif // BEHAVIOR_STACK_HPP
