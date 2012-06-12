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


#ifndef NESTABLE_RECEIVE_POLICY_HPP
#define NESTABLE_RECEIVE_POLICY_HPP

#include <list>
#include <memory>

#include "cppa/behavior.hpp"
#include "cppa/partial_function.hpp"
#include "cppa/detail/filter_result.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

namespace cppa { namespace detail {

class nestable_receive_policy {

 public:

    enum handle_message_result {
        hm_timeout_msg,
        hm_skip_msg,
        hm_drop_msg,
        hm_cache_msg,
        hm_success
    };

    template<class Client, class FunOrBehavior>
    bool invoke_from_cache(Client* client, FunOrBehavior& fun) {
        auto i = m_cache.begin();
        auto e = m_cache.end();
        while (i != e) {
            switch (this->handle_message(client, *(*i), fun)) {
                case hm_success: {
                    client->release_node(i->release());
                    m_cache.erase(i);
                    return true;
                }
                case hm_drop_msg: {
                    client->release_node(i->release());
                    i = m_cache.erase(i);
                    break;
                }
                case hm_skip_msg:
                case hm_cache_msg: {
                    ++i;
                    break;
                }
                default: {
                    CPPA_CRITICAL("illegal result of handle_message");
                }
            }
        }
        return false;
    }

    template<class Client, class FunOrBehavior>
    bool invoke(Client* client, recursive_queue_node* node, FunOrBehavior& fun) {
        switch (this->handle_message(client, *node, fun)) {
            case hm_success: {
                client->release_node(node);
                return true;
            }
            case hm_drop_msg: {
                client->release_node(node);
                break;
            }
            case hm_cache_msg: {
                m_cache.emplace_back(node);
                break;
            }
            case hm_skip_msg: {
                CPPA_CRITICAL("received a marked node");
            }
            default: {
                CPPA_CRITICAL("illegal result of handle_message");
            }
        }
        return false;
    }

    template<class Client>
    void receive(Client* client, partial_function& fun) {
        if (invoke_from_cache(client, fun) == false) {
            while (invoke(client, client->receive_node(), fun) == false) { }
        }
    }

 private:

    std::list<std::unique_ptr<recursive_queue_node> > m_cache;

    inline void handle_timeout(behavior& bhvr) {
        bhvr.handle_timeout();
    }

    inline void handle_timeout(partial_function&) {
        CPPA_CRITICAL("handle_timeout(partial_function&)");
    }

    template<class Client, class FunOrBehavior>
    handle_message_result handle_message(Client* client,
                                         recursive_queue_node& node,
                                         FunOrBehavior& fun) {
        if (node.marked) {
            return hm_skip_msg;
        }
        switch (client->filter_msg(node.msg)) {
            case normal_exit_signal:
            case expired_timeout_message: {
                return hm_drop_msg;
            }
            case timeout_message: {
                handle_timeout(fun);
                return hm_success;
            }
            case ordinary_message: {
                std::swap(client->m_last_dequeued, node.msg);
                std::swap(client->m_last_sender, node.sender);
                client->push_timeout();
                node.marked = true;
                if (fun(client->m_last_dequeued)) {
                    client->m_last_dequeued.reset();
                    client->m_last_sender.reset();
                    return hm_success;
                }
                // no match (restore client members)
                std::swap(client->m_last_dequeued, node.msg);
                std::swap(client->m_last_sender, node.sender);
                client->pop_timeout();
                node.marked = false;
                return hm_cache_msg;
            }
            default: {
                CPPA_CRITICAL("illegal result of filter_msg");
            }
        }
    }

};

} } // namespace cppa::detail

#endif // NESTABLE_RECEIVE_POLICY_HPP
