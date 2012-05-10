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


#ifndef NESTABLE_RECEIVE_ACTOR_HPP
#define NESTABLE_RECEIVE_ACTOR_HPP

#include <memory>

#include "cppa/config.hpp"
#include "cppa/behavior.hpp"
#include "cppa/partial_function.hpp"
#include "cppa/detail/filter_result.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

namespace cppa { namespace detail {

template<class Derived, class Base>
class nestable_receive_actor : public Base
{

 protected:

    enum handle_message_result
    {
        hm_timeout_msg,
        hm_skip_msg,
        hm_drop_msg,
        hm_cache_msg,
        hm_success
    };

    template<class FunOrBehavior>
    bool invoke_from_cache(FunOrBehavior& fun)
    {
        auto i = m_cache.begin();
        auto e = m_cache.end();
        while (i != e)
        {
            switch (this->handle_message(*(*i), fun))
            {
                case hm_success:
                {
                    this->release_node(i->release());
                    m_cache.erase(i);
                    return true;
                }
                case hm_drop_msg:
                {
                    this->release_node(i->release());
                    i = m_cache.erase(i);
                    break;
                }
                case hm_skip_msg:
                case hm_cache_msg:
                {
                    ++i;
                    break;
                }
                default:
                {
                    CPPA_CRITICAL("illegal result of handle_message");
                }
            }
        }
        return false;
    }

    template<class FunOrBehavior>
    bool invoke(recursive_queue_node* node, FunOrBehavior& fun)
    {
        switch (this->handle_message(*node, fun))
        {
            case hm_success:
            {
                this->release_node(node);
                return true;
            }
            case hm_drop_msg:
            {
                this->release_node(node);
                break;
            }
            case hm_cache_msg:
            {
                m_cache.emplace_back(node);
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

    std::list<std::unique_ptr<recursive_queue_node> > m_cache;

    inline Derived* dthis()
    {
        return static_cast<Derived*>(this);
    }

    inline Derived const* dthis() const
    {
        return static_cast<Derived const*>(this);
    }

    void handle_timeout(behavior& bhvr)
    {
        bhvr.handle_timeout();
    }

    void handle_timeout(partial_function&)
    {
        CPPA_CRITICAL("handle_timeout(partial_function&)");
    }

    template<class FunOrBehavior>
    handle_message_result handle_message(recursive_queue_node& node,
                                         FunOrBehavior& fun)
    {
        if (node.marked)
        {
            return hm_skip_msg;
        }
        switch (dthis()->filter_msg(node.msg))
        {
            case normal_exit_signal:
            case expired_timeout_message:
            {
                return hm_drop_msg;
            }
            case timeout_message:
            {
                handle_timeout(fun);
                return hm_success;
            }
            case ordinary_message:
            {
                break;
            }
            default:
            {
                CPPA_CRITICAL("illegal result of filter_msg");
            }
        }
        std::swap(this->m_last_dequeued, node.msg);
        std::swap(this->m_last_sender, node.sender);
        dthis()->push_timeout();
        node.marked = true;
        if (fun(this->m_last_dequeued))
        {
            this->m_last_dequeued.reset();
            this->m_last_sender.reset();
            return hm_success;
        }
        // no match (restore members)
        std::swap(this->m_last_dequeued, node.msg);
        std::swap(this->m_last_sender, node.sender);
        dthis()->pop_timeout();
        node.marked = false;
        return hm_cache_msg;
    }

};

} } // namespace cppa::detail

#endif // NESTABLE_RECEIVE_ACTOR_HPP
