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

// implements linking and monitoring for actors
template<class Base>
class abstract_actor : public Base
{
    typedef detail::lock_guard<detail::mutex> guard_type;
    //typedef std::lock_guard<std::mutex> guard_type;
    typedef std::unique_ptr<attachable> attachable_ptr;

    // true if the associated thread has finished execution
    std::atomic<std::uint32_t> m_exit_reason;

    // guards access to m_exited, m_subscriptions and m_links
    //std::mutex m_mtx;
    detail::mutex m_mtx;

    // manages actor links
    std::list<actor_ptr> m_links;

    std::vector<attachable_ptr> m_attachables;

 public:

    class queue_node_ptr;
    struct queue_node_deallocator;

    struct queue_node
    {
        friend class abstract_actor;
        friend class queue_node_ptr;
        friend struct queue_node_deallocator;

        queue_node* next;
        std::atomic<queue_node*>* owner;
        actor_ptr sender;
        any_tuple msg;

     private: // you have to be a friend to create or destroy a node

        inline ~queue_node() { }
        queue_node() : next(nullptr), owner(nullptr) { }
        queue_node(actor* from, any_tuple&& content)
            : next(nullptr), owner(nullptr), sender(from), msg(std::move(content))
        {
        }
        queue_node(actor* from, any_tuple const& content)
            : next(nullptr), owner(nullptr), sender(from), msg(content)
        {
        }
    };

    struct queue_node_deallocator
    {
        inline void operator()(queue_node* ptr)
        {
            if (ptr)
            {
                if (ptr->owner != nullptr)
                {
                    ptr->sender.reset();
                    ptr->msg = any_tuple();
                    auto owner = ptr->owner;
                    ptr->next = owner->load();
                    for (;;)
                    {
                        if (owner->compare_exchange_weak(ptr->next, ptr)) return;
                    }
                }
                else
                {
                    delete ptr;
                }
            }
        }
    };

    class queue_node_ptr
    {

        queue_node* m_ptr;
        queue_node_deallocator d;

     public:

        inline queue_node_ptr(queue_node* ptr = nullptr) : m_ptr(ptr)
        {
        }

        inline queue_node_ptr(queue_node_ptr&& other) : m_ptr(other.m_ptr)
        {
            other.m_ptr = nullptr;
        }

        inline ~queue_node_ptr()
        {
            d(m_ptr);
        }

        inline queue_node* operator->() { return m_ptr; }

        queue_node* release()
        {
            auto result = m_ptr;
            m_ptr = nullptr;
            return result;
        }

        inline void reset(queue_node* ptr = nullptr)
        {
            d(m_ptr);
            m_ptr = ptr;
        }

        inline operator bool() const { return m_ptr != nullptr; }

    };

 protected:

    queue_node m_prefetched_nodes[10];
    std::atomic<queue_node*> m_prefetched;
    util::single_reader_queue<queue_node,queue_node_deallocator> m_mailbox;

 private:

    // @pre m_mtx.locked()
    bool exited() const
    {
        return m_exit_reason.load() != exit_reason::not_exited;
    }

    template<class List, typename Element>
    bool unique_insert(List& lst, Element const& e)
    {
        auto end = lst.end();
        auto i = std::find(lst.begin(), end, e);
        if (i == end)
        {
            lst.push_back(e);
            return true;
        }
        return false;
    }

    template<class List, typename Iterator, typename Element>
    int erase_all(List& lst, Iterator begin, Iterator end, Element const& e)
    {
        auto i = std::find(begin, end, e);
        if (i != end)
        {
            return 1 + erase_all(lst, lst.erase(i), end, e);
        }
        return 0;
    }

    template<class List, typename Element>
    int erase_all(List& lst, Element const& e)
    {
        return erase_all(lst, lst.begin(), lst.end(), e);
    }

 protected:

    template<typename T>
    queue_node* fetch_node(actor* sender, T&& msg)
    {
        queue_node* result = m_prefetched.load();
        while (result)
        {
            queue_node* next = result->next;
            if (m_prefetched.compare_exchange_weak(result, next))
            {
                result->next = nullptr;
                result->sender.reset(sender);
                result->msg = std::forward<T>(msg);
                return result;
            }
        }
        return new queue_node(sender, std::forward<T>(msg));
    }

    template<typename... Args>
    abstract_actor(Args&&... args) : Base(std::forward<Args>(args)...)
                                   , m_exit_reason(exit_reason::not_exited)
    {
        for (int i = 0; i < 9; ++i)
        {
            m_prefetched_nodes[i].next = &(m_prefetched_nodes[i+1]);
            m_prefetched_nodes[i].owner = &m_prefetched;
        }
        m_prefetched_nodes[9].owner = &m_prefetched;
        m_prefetched.store(m_prefetched_nodes);
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
        if (!mlinks.empty())
        {
            // send exit messages
            for (actor_ptr& aptr : mlinks)
            {
                aptr->enqueue(this, make_tuple(atom(":Exit"), reason));
            }
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
            if (exited())
            {
                other->enqueue(this, make_tuple(atom(":Exit"),
                                                m_exit_reason.load()));
            }
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
        if (other && !exited() && other->remove_backlink(this))
        {
            return erase_all(m_links, other) > 0;
        }
        return false;
    }

 public:

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
            auto i = std::find_if(m_attachables.begin(),
                                  end,
                                  [&](attachable_ptr& ptr)
                                  {
                                      return ptr->matches(what);
                                  });
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
            return erase_all(m_links, other) > 0;
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
                return unique_insert(m_links, other);
            }
        }
        // send exit message without lock
        if (reason != exit_reason::not_exited)
        {
            other->enqueue(this, make_tuple(atom(":Exit"), reason));
        }
        return false;
    }

};

} // namespace cppa

#endif // ABSTRACT_ACTOR_HPP
