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


#ifndef NESTABLE_INVOKE_POLICY_HPP
#define NESTABLE_INVOKE_POLICY_HPP

#include <list>
#include <memory>

#include "cppa/behavior.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/partial_function.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

namespace cppa { namespace detail {

template<class FilterPolicy>
class nestable_invoke_policy
{

 public:

    typedef std::unique_ptr<recursive_queue_node> queue_node_ptr;

    template<typename... Args>
    nestable_invoke_policy(local_actor* parent, Args&&... args)
        : m_last_dequeued(parent->last_dequeued())
        , m_last_sender(parent->last_sender())
        , m_filter_policy(std::forward<Args>(args)...)
    {
    }

    template<typename... Args>
    bool invoke_from_cache(partial_function& fun, Args... args)
    {
        auto i = m_cache.begin();
        auto e = m_cache.end();
        while (i != e)
        {
            switch (handle_message(*(*i), fun, args...))
            {
                case hm_drop_msg:
                {
                    i = m_cache.erase(i);
                    break;
                }
                case hm_success:
                {
                    m_cache.erase(i);
                    return true;
                }
                case hm_skip_msg:
                case hm_cache_msg:
                {
                    ++i;
                    break;
                }
                default: CPPA_CRITICAL("illegal result of handle_message");
            }
        }
        return false;
    }

    template<typename... Args>
    bool invoke(queue_node_ptr& ptr, partial_function& fun, Args... args)
    {
        switch (handle_message(*ptr, fun, args...))
        {
            case hm_drop_msg:
            {
                break;
            }
            case hm_success:
            {
                return true; // done
            }
            case hm_cache_msg:
            {
                m_cache.push_back(std::move(ptr));
                break;
            }
            case hm_skip_msg:
            {
                CPPA_CRITICAL("received a marked node");
            }
            default:
            {
                CPPA_CRITICAL("illegal result of handle_message");
            }
        }
        return false;
    }

 private:

    enum handle_message_result
    {
        hm_timeout_msg,
        hm_skip_msg,
        hm_drop_msg,
        hm_cache_msg,
        hm_success
    };

    any_tuple& m_last_dequeued;
    actor_ptr& m_last_sender;
    FilterPolicy m_filter_policy;
    std::list<queue_node_ptr> m_cache;

    template<typename... Args>
    handle_message_result handle_message(recursive_queue_node& node,
                                         partial_function& fun,
                                         Args... args)
    {
        if (node.marked)
        {
            return hm_skip_msg;
        }
        if (m_filter_policy(node.msg, args...))
        {
            return hm_drop_msg;
        }
        std::swap(m_last_dequeued, node.msg);
        std::swap(m_last_sender, node.sender);
        {
            typename recursive_queue_node::guard qguard{&node};
            if (fun(m_last_dequeued))
            {
                // client calls erase(iter)
                qguard.release();
                m_last_dequeued.reset();
                m_last_sender.reset();
                return hm_success;
            }
        }
        // no match (restore members)
        std::swap(m_last_dequeued, node.msg);
        std::swap(m_last_sender, node.sender);
        return hm_cache_msg;
    }

};

} }

#endif // NESTABLE_INVOKE_POLICY_HPP
