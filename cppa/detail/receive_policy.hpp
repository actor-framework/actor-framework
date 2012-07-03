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


#ifndef CPPA_NESTABLE_RECEIVE_POLICY_HPP
#define CPPA_NESTABLE_RECEIVE_POLICY_HPP

#include <list>
#include <memory>
#include <type_traits>

#include "cppa/behavior.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/partial_function.hpp"
#include "cppa/detail/filter_result.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

namespace cppa { namespace detail {

enum receive_policy_flag {
    // receives can be nested
    rp_nestable,
    // receives are guaranteed to be sequential
    rp_sequential
};

template<receive_policy_flag X>
struct rp_flag { typedef std::integral_constant<receive_policy_flag, X> type; };

class receive_policy {

 public:

    enum handle_message_result {
        hm_timeout_msg,
        hm_skip_msg,
        hm_drop_msg,
        hm_cache_msg,
        hm_msg_handled
    };

    template<class Client, class FunOrBehavior>
    bool invoke_from_cache(Client* client, FunOrBehavior& fun) {
        std::integral_constant<receive_policy_flag, Client::receive_flag> token;
        auto i = m_cache.begin();
        auto e = m_cache.end();
        while (i != e) {
            switch (this->handle_message(client, i->get(), fun, token)) {
                case hm_msg_handled: {
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
    bool invoke(Client* client, recursive_queue_node* node, FunOrBehavior& fun){
        std::integral_constant<receive_policy_flag, Client::receive_flag> token;
        switch (this->handle_message(client, node, fun, token)) {
            case hm_msg_handled: {
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

    template<class Client>
    void receive(Client* client, behavior& bhvr) {
        partial_function& fun = bhvr;
        if (bhvr.timeout().valid() == false) {
            receive(client, fun);
        }
        else if (invoke_from_cache(client, fun) == false) {
            if (bhvr.timeout().is_zero()) {
                recursive_queue_node* e = nullptr;
                while ((e = client->try_receive_node()) != nullptr) {
                    CPPA_REQUIRE(e->marked == false);
                    if (invoke(client, e, bhvr)) {
                        return; // done
                    }
                }
                handle_timeout(client, bhvr);
            }
            else {
                auto timeout = client->init_timeout(bhvr.timeout());
                recursive_queue_node* e = nullptr;
                while ((e = client->try_receive_node(timeout)) != nullptr) {
                    CPPA_REQUIRE(e->marked == false);
                    if (invoke(client, e, bhvr)) {
                        return; // done
                    }
                }
                handle_timeout(client, bhvr);
            }
        }
    }

 private:

    typedef typename rp_flag<rp_nestable>::type nestable;
    typedef typename rp_flag<rp_sequential>::type sequential;

    std::list<std::unique_ptr<recursive_queue_node> > m_cache;

    template<class Client>
    inline void handle_timeout(Client* client, behavior& bhvr) {
        client->handle_timeout(bhvr);
    }

    template<class Client>
    inline void handle_timeout(Client*, partial_function&) {
        CPPA_CRITICAL("handle_timeout(partial_function&)");
    }

    template<class Client>
    filter_result filter_msg(Client* client, recursive_queue_node* node) {
        const any_tuple& msg = node->msg;
        bool is_sync_msg = node->seq_id != 0;
        auto& arr = detail::static_types_array<atom_value, std::uint32_t>::arr;
        if (   msg.size() == 2
            && msg.type_at(0) == arr[0]
            && msg.type_at(1) == arr[1]) {
            auto v0 = msg.get_as<atom_value>(0);
            auto v1 = msg.get_as<std::uint32_t>(1);
            if (v0 == atom("EXIT")) {
                CPPA_REQUIRE(is_sync_msg == false);
                if (client->m_trap_exit == false) {
                    if (v1 != exit_reason::normal) {
                        // TODO: check for possible memory leak here
                        // ('node' might not get destroyed)
                        client->quit(v1);
                    }
                    return normal_exit_signal;
                }
            }
            else if (v0 == atom("TIMEOUT")) {
                CPPA_REQUIRE(is_sync_msg == false);
                return client->waits_for_timeout(v1) ? timeout_message
                                                     : expired_timeout_message;
            }
        }
        constexpr std::uint64_t is_response_mask = 0x8000000000000000;
        constexpr std::uint64_t  request_id_mask = 0x7FFFFFFFFFFFFFFF;
        // first bit is 1 if this is a response message
        if (   is_sync_msg
            && (node->seq_id & is_response_mask) != 0
            && (node->seq_id &  request_id_mask) != client->m_sync_request_id) {
            return expired_sync_enqueue;
        }
        return ordinary_message;
    }

    template<class Client, class FunOrBehavior>
    handle_message_result handle_message(Client* client,
                                         recursive_queue_node* node,
                                         FunOrBehavior& fun,
                                         nestable) {
        if (node->marked) {
            return hm_skip_msg;
        }
        switch (this->filter_msg(client, node)) {
            case normal_exit_signal:
            case expired_sync_enqueue:
            case expired_timeout_message: {
                return hm_drop_msg;
            }
            case timeout_message: {
                handle_timeout(client, fun);
                return hm_msg_handled;
            }
            case ordinary_message: {
                auto previous_node = client->m_current_node;
                client->m_current_node = node;
                client->push_timeout();
                node->marked = true;
                if (fun(node->msg)) {
                    client->m_current_node = &(client->m_dummy_node);
                    return hm_msg_handled;
                }
                // no match (restore client members)
                client->m_current_node = previous_node;
                client->pop_timeout();
                node->marked = false;
                return hm_cache_msg;
            }
            default: CPPA_CRITICAL("illegal result of filter_msg");
        }
    }

    template<class Client, class FunOrBehavior>
    handle_message_result handle_message(Client* client,
                                         recursive_queue_node* node,
                                         FunOrBehavior& fun,
                                         sequential) {
        CPPA_REQUIRE(node->marked == false);
        switch (this->filter_msg(client, node)) {
            case normal_exit_signal:
            case expired_sync_enqueue:
            case expired_timeout_message: {
                return hm_drop_msg;
            }
            case timeout_message: {
                handle_timeout(client, fun);
                return hm_msg_handled;
            }
            case ordinary_message: {
                auto previous_node = client->m_current_node;
                client->m_current_node = node;
                if (fun(node->msg)) {
                    client->m_current_node = &(client->m_dummy_node);
                    // we definitely don't have a pending timeout now
                    client->m_has_pending_timeout_request = false;
                    return hm_msg_handled;
                }
                // no match, restore members
                client->m_current_node = previous_node;
                return hm_cache_msg;
            }
            default: CPPA_CRITICAL("illegal result of filter_msg");
        }
    }

};

} } // namespace cppa::detail

#endif // CPPA_NESTABLE_RECEIVE_POLICY_HPP
