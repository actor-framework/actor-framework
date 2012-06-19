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


#ifndef CPPA_ABSTRACT_ACTOR_HPP
#define CPPA_ABSTRACT_ACTOR_HPP

#include "cppa/config.hpp"

#include <list>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>
#include <thread>
#include <algorithm>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/attachable.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/util/shared_spinlock.hpp"

#include "cppa/detail/recursive_queue_node.hpp"
#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { class self_type; }

namespace cppa { namespace detail {

/**
 * @brief Implements linking and monitoring for actors.
 * @tparam Base Either {@link cppa::actor actor}
 *              or {@link cppa::local_actor local_actor}.
 */
template<class Base>
class abstract_actor : public Base {

    friend class self_type;

    typedef std::unique_ptr<attachable> attachable_ptr;
    typedef std::lock_guard<std::mutex> guard_type;

 public:

    typedef detail::recursive_queue_node mailbox_element;
    typedef intrusive::single_reader_queue<mailbox_element> mailbox_type;

    bool attach(attachable* ptr) { // override
        if (ptr == nullptr) {
            guard_type guard(m_mtx);
            return m_exit_reason.load() == exit_reason::not_exited;
        }
        else {
            attachable_ptr uptr(ptr);
            std::uint32_t reason;
            { // lifetime scope of guard
                guard_type guard(m_mtx);
                reason = m_exit_reason.load();
                if (reason == exit_reason::not_exited) {
                    m_attachables.push_back(std::move(uptr));
                    return true;
                }
            }
            uptr->actor_exited(reason);
            return false;
        }
    }

    void detach(const attachable::token& what) { // override
        attachable_ptr uptr;
        { // lifetime scope of guard
            guard_type guard(m_mtx);
            auto end = m_attachables.end();
            auto i = std::find_if(
                        m_attachables.begin(), end,
                        [&](attachable_ptr& p) { return p->matches(what); });
            if (i != end) {
                uptr = std::move(*i);
                m_attachables.erase(i);
            }
        }
        // uptr will be destroyed here, without locked mutex
    }

    void link_to(intrusive_ptr<actor>& other) { // override
        (void) link_to_impl(other);
    }

    void unlink_from(intrusive_ptr<actor>& other) { // override
        (void) unlink_from_impl(other);
    }

    bool remove_backlink(intrusive_ptr<actor>& other) { // override
        if (other && other != this) {
            guard_type guard(m_mtx);
            auto i = std::find(m_links.begin(), m_links.end(), other);
            if (i != m_links.end()) {
                m_links.erase(i);
                return true;
            }
        }
        return false;
    }

    bool establish_backlink(intrusive_ptr<actor>& other) { // override
        std::uint32_t reason = exit_reason::not_exited;
        if (other && other != this) {
            guard_type guard(m_mtx);
            reason = m_exit_reason.load();
            if (reason == exit_reason::not_exited) {
                auto i = std::find(m_links.begin(), m_links.end(), other);
                if (i == m_links.end()) {
                    m_links.push_back(other);
                    return true;
                }
            }
        }
        // send exit message without lock
        if (reason != exit_reason::not_exited) {
            other->enqueue(this, make_cow_tuple(atom("EXIT"), reason));
        }
        return false;
    }

 protected:

    mailbox_type m_mailbox;
    util::fixed_vector<mailbox_element*, 10> m_nodes;
    util::shared_spinlock m_nodes_lock;

    typedef std::lock_guard<util::shared_spinlock> lock_type;

    inline mailbox_element* fetch_node(actor* sender, any_tuple msg) {
        mailbox_element* result = nullptr;
        { // lifetime scope of guard
            lock_type guard{m_nodes_lock};
            if (m_nodes.not_empty()) {
                result = m_nodes.back();
                m_nodes.pop_back();
            }
        }
        if (result) {
            result->next = nullptr;
            result->marked = false;
            result->sender = sender;
            result->msg = std::move(msg);
        }
        else {
            result = new mailbox_element(sender, std::move(msg));
        }
        return result;
    }

    inline void release_node(mailbox_element* node) {
        { // lifetime scope of guard
            lock_type guard{m_nodes_lock};
            if (m_nodes.full() == false) {
                m_nodes.push_back(node);
                return;
            }
        }
        delete node;
    }

    template<typename... Args>
    abstract_actor(Args&&... args) : Base(std::forward<Args>(args)...)
                                   , m_exit_reason(exit_reason::not_exited) {
        // pre-allocate some nodes
        for (size_t i = 0; i < m_nodes.max_size() / 2; ++i) {
            m_nodes.push_back(new mailbox_element);
        }
    }

    void cleanup(std::uint32_t reason) {
        if (reason == exit_reason::not_exited) return;
        decltype(m_links) mlinks;
        decltype(m_attachables) mattachables;
        { // lifetime scope of guard
            guard_type guard(m_mtx);
            if (m_exit_reason != exit_reason::not_exited) {
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
        for (actor_ptr& aptr : mlinks) {
            aptr->enqueue(this, make_cow_tuple(atom("EXIT"), reason));
        }
        for (attachable_ptr& ptr : mattachables) {
            ptr->actor_exited(reason);
        }
    }

    bool link_to_impl(intrusive_ptr<actor>& other) {
        if (other && other != this) {
            guard_type guard(m_mtx);
            // send exit message if already exited
            if (exited()) {
                other->enqueue(this, make_cow_tuple(atom("EXIT"),
                                                m_exit_reason.load()));
            }
            // add link if not already linked to other
            // (checked by establish_backlink)
            else if (other->establish_backlink(this)) {
                m_links.push_back(other);
                return true;
            }
        }
        return false;
    }

    bool unlink_from_impl(intrusive_ptr<actor>& other) {
        guard_type guard(m_mtx);
        // remove_backlink returns true if this actor is linked to other
        if (other && !exited() && other->remove_backlink(this)) {
            auto i = std::find(m_links.begin(), m_links.end(), other);
            CPPA_REQUIRE(i != m_links.end());
            m_links.erase(i);
            return true;
        }
        return false;
    }

 private:

    // @pre m_mtx.locked()
    bool exited() const {
        return m_exit_reason.load() != exit_reason::not_exited;
    }

    // true if the associated thread has finished execution
    std::atomic<std::uint32_t> m_exit_reason;
    // guards access to m_exited, m_subscriptions and m_links
    std::mutex m_mtx;
    // links to other actors
    std::vector<actor_ptr> m_links;
    // code that is executed on cleanup
    std::vector<attachable_ptr> m_attachables;

};

} } // namespace cppa::detail

#endif // CPPA_ABSTRACT_ACTOR_HPP
