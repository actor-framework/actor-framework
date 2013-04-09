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
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef CPPA_ACTOR_COMPANION_MIXIN_HPP
#define CPPA_ACTOR_COMPANION_MIXIN_HPP

#include <mutex>  // std::lock_guard
#include <memory> // std::unique_ptr

#include "cppa/self.hpp"
#include "cppa/actor.hpp"
#include "cppa/extend.hpp"
#include "cppa/match_expr.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/partial_function.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

#include "cppa/detail/memory.hpp"

namespace cppa {

/**
 * @brief Adds co-existing objects (companions) to a class,
 *        which serve as gateways, thereby enabling libcppa's message passing.
 */
template<typename Base>
class actor_companion_mixin : public Base {

    typedef Base super;

 public:

    typedef std::unique_ptr<mailbox_element,detail::disposer> message_pointer;

    template<typename... Ts>
    actor_companion_mixin(Ts&&... args) : super(std::forward<Ts>(args)...) {
        m_self.reset(detail::memory::create<companion>(this));
    }

    ~actor_companion_mixin() {
        m_self->disconnect();
    }

    /**
     * @brief Returns a smart pointer to the companion object.
     */
    inline actor_ptr as_actor() const { return m_self; }

 protected:

    /**
     * @brief This callback is invoked by the companion object whenever a
     *        new messages arrives.
     * @warning Implementation has to be thread-safe.
     */
    virtual void new_message(message_pointer ptr) = 0;

    /**
     * @brief Defines the message handler.
     * @note While the message handler is invoked, @p self will point
     *       to the companion object to enable send() and reply().
     */
    template<typename... Ts>
    void set_message_handler(Ts&&... args) {
        m_message_handler = match_expr_convert(std::forward<Ts>(args)...);
    }

    /**
     * @brief Invokes the message handler with @p msg.
     * @note While the message handler is invoked, @p self will point
     *       to the companion object to enable send() and reply().
     */
    void handle_message(const message_pointer& msg) {
        if (!msg) return;
        scoped_self_setter sss(m_self.get());
        m_self->m_current_node = msg.get();
        auto f = m_message_handler; // make sure ref count >= 2
        f(msg->msg);
    }

 private:

    class companion : public local_actor {

        friend class actor_companion_mixin;

        typedef util::shared_spinlock lock_type;

     public:

        companion(actor_companion_mixin* parent) : m_parent(parent) { }

        void disconnect() {
            { // lifetime scope of guard
                std::lock_guard<lock_type> guard(m_lock);
                m_parent = nullptr;
            }
            cleanup(exit_reason::normal);
        }

        void enqueue(const actor_ptr& sender, any_tuple msg) {
            util::shared_lock_guard<lock_type> guard(m_lock);
            if (m_parent) push_message(sender, std::move(msg));
        }

        void sync_enqueue(const actor_ptr& sender,
                          message_id id,
                          any_tuple msg) {
            util::shared_lock_guard<lock_type> guard(m_lock);
            if (m_parent) push_message(sender, std::move(msg), id);
        }

        bool initialized() const { return true; }

        void quit(std::uint32_t) {
            throw std::runtime_error("Using actor_companion_mixin::quit "
                                     "is prohibited; use Qt's widget "
                                     "management instead.");
        }

        void dequeue(behavior&) {
            throw_no_recv();
        }

        void dequeue_response(behavior&, message_id) {
            throw_no_recv();
        }

        void become_waiting_for(behavior, message_id) {
            throw_no_become();
        }

     protected:

        void do_become(behavior&&, bool) { throw_no_become(); }

     private:

        template<typename... Ts>
        void push_message(Ts&&... args) {
            message_pointer ptr;
            ptr.reset(detail::memory::create<mailbox_element>(std::forward<Ts>(args)...));
            m_parent->new_message(std::move(ptr));
        }

        void throw_no_become() {
            throw std::runtime_error("actor_companion_companion "
                                     "does not support libcppa's "
                                     "become() API");
        }

        void throw_no_recv() {
            throw std::runtime_error("actor_companion_companion "
                                     "does not support libcppa's "
                                     "receive() API");
        }

        // guards access to m_parent
        lock_type m_lock;

        // set to nullptr if this companion became detached
        actor_companion_mixin* m_parent;

    };

    // used as 'self' before calling message handler
    intrusive_ptr<companion> m_self;

    // user-defined message handler for incoming messages
    partial_function m_message_handler;

};

} // namespace cppa

#endif // CPPA_ACTOR_COMPANION_MIXIN_HPP
