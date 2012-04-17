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


#ifndef ABSTRACT_ACTOR_HPP
#define ABSTRACT_ACTOR_HPP

#include "cppa/config.hpp"

#include <list>
#include <atomic>
#include <vector>
#include <memory>
#include <algorithm>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/attachable.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/detail/thread.hpp"

namespace cppa {

/**
 * @brief Implements linking and monitoring for actors.
 * @tparam Base Either {@link cppa::actor actor}
 *              or {@link cppa::local_actor local_actor}.
 */
template<class Base>
class abstract_actor : public Base
{

    typedef std::unique_ptr<attachable> attachable_ptr;
    typedef detail::lock_guard<detail::mutex> guard_type;

 public:

    struct queue_node
    {
        queue_node* next;   // intrusive next pointer
        bool marked;        // denotes if this node is currently processed
        actor_ptr sender;
        any_tuple msg;
        queue_node() : next(nullptr), marked(false) { }
        queue_node(actor* from, any_tuple content)
            : next(nullptr), marked(false), sender(from), msg(std::move(content))
        {
        }
    };

    struct queue_node_guard
    {
        queue_node* m_node;
        queue_node_guard(queue_node* ptr) : m_node(ptr) { ptr->marked = true; }
        inline void release() { m_node = nullptr; }
        ~queue_node_guard() { if (m_node) m_node->marked = false; }
    };

    typedef intrusive::single_reader_queue<queue_node> mailbox_type;
    typedef std::unique_ptr<queue_node> queue_node_ptr;
    typedef typename mailbox_type::cache_type mailbox_cache_type;
    typedef typename mailbox_cache_type::iterator queue_node_iterator;

    bool attach(attachable* ptr) /*override*/
    {
        if (ptr == nullptr)
        {
            guard_type guard(m_mtx);
            return m_exit_reason.load() == exit_reason::not_exited;
        }
        else
        {
            attachable_ptr uptr(ptr);
            std::uint32_t reason;
            // lifetime scope of guard
            {
                guard_type guard(m_mtx);
                reason = m_exit_reason.load();
                if (reason == exit_reason::not_exited)
                {
                    m_attachables.push_back(std::move(uptr));
                    return true;
                }
            }
            uptr->actor_exited(reason);
            return false;
        }
    }

    void detach(attachable::token const& what) /*override*/
    {
        attachable_ptr uptr;
        // lifetime scope of guard
        {
            guard_type guard(m_mtx);
            auto end = m_attachables.end();
            auto i = std::find_if(
                        m_attachables.begin(), end,
                        [&](attachable_ptr& p) { return p->matches(what); });
            if (i != end)
            {
                uptr = std::move(*i);
                m_attachables.erase(i);
            }
        }
        // uptr will be destroyed here, without locked mutex
    }

    void link_to(intrusive_ptr<actor>& other) /*override*/
    {
        (void) link_to_impl(other);
    }

    void unlink_from(intrusive_ptr<actor>& other) /*override*/
    {
        (void) unlink_from_impl(other);
    }

    bool remove_backlink(intrusive_ptr<actor>& other) /*override*/
    {
        if (other && other != this)
        {
            guard_type guard(m_mtx);
            auto i = std::find(m_links.begin(), m_links.end(), other);
            if (i != m_links.end())
            {
                m_links.erase(i);
                return true;
            }
        }
        return false;
    }

    bool establish_backlink(intrusive_ptr<actor>& other) /*override*/
    {
        std::uint32_t reason = exit_reason::not_exited;
        if (other && other != this)
        {
            guard_type guard(m_mtx);
            reason = m_exit_reason.load();
            if (reason == exit_reason::not_exited)
            {
                auto i = std::find(m_links.begin(), m_links.end(), other);
                if (i == m_links.end())
                {
                    m_links.push_back(other);
                    return true;
                }
            }
        }
        // send exit message without lock
        if (reason != exit_reason::not_exited)
        {
            other->enqueue(this, make_cow_tuple(atom(":Exit"), reason));
        }
        return false;
    }

 protected:

    mailbox_type m_mailbox;

    template<typename T>
    inline queue_node* fetch_node(actor* sender, T&& msg)
    {
        return new queue_node(sender, std::forward<T>(msg));
    }

    template<typename... Args>
    abstract_actor(Args&&... args) : Base(std::forward<Args>(args)...)
                                   , m_exit_reason(exit_reason::not_exited)
    {
    }

    void cleanup(std::uint32_t reason)
    {
        if (reason == exit_reason::not_exited) return;
        decltype(m_links) mlinks;
        decltype(m_attachables) mattachables;
        // lifetime scope of guard
        {
            guard_type guard(m_mtx);
            if (m_exit_reason != exit_reason::not_exited)
            {
                // already exited
                return;
            }
            m_exit_reason = reason;
            mlinks = std::move(m_links);
            mattachables = std::move(m_attachables);
            // make sure lists are definitely empty
            m_links.clear();
            m_attachables.clear();
        }
        // send exit messages
        for (actor_ptr& aptr : mlinks)
        {
            aptr->enqueue(this, make_cow_tuple(atom(":Exit"), reason));
        }
        for (attachable_ptr& ptr : mattachables)
        {
            ptr->actor_exited(reason);
        }
    }

    bool link_to_impl(intrusive_ptr<actor>& other)
    {
        if (other && other != this)
        {
            guard_type guard(m_mtx);
            // send exit message if already exited
            if (exited())
            {
                other->enqueue(this, make_cow_tuple(atom(":Exit"),
                                                m_exit_reason.load()));
            }
            // add link if not already linked to other
            // (checked by establish_backlink)
            else if (other->establish_backlink(this))
            {
                m_links.push_back(other);
                return true;
            }
        }
        return false;
    }

    bool unlink_from_impl(intrusive_ptr<actor>& other)
    {
        guard_type guard(m_mtx);
        // remove_backlink returns true if this actor is linked to other
        if (other && !exited() && other->remove_backlink(this))
        {
            auto i = std::find(m_links.begin(), m_links.end(), other);
            CPPA_REQUIRE(i != m_links.end());
            m_links.erase(i);
            return true;
        }
        return false;
    }

 private:

    // @pre m_mtx.locked()
    bool exited() const
    {
        return m_exit_reason.load() != exit_reason::not_exited;
    }

    // true if the associated thread has finished execution
    std::atomic<std::uint32_t> m_exit_reason;
    // guards access to m_exited, m_subscriptions and m_links
    detail::mutex m_mtx;
    // links to other actors
    std::vector<actor_ptr> m_links;
    // code that is executed on cleanup
    std::vector<attachable_ptr> m_attachables;

};

} // namespace cppa

#endif // ABSTRACT_ACTOR_HPP
