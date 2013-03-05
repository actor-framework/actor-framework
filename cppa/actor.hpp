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


#ifndef CPPA_ACTOR_HPP
#define CPPA_ACTOR_HPP

#include <mutex>
#include <atomic>
#include <memory>
#include <vector>
#include <cstdint>
#include <type_traits>

#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/cppa_fwd.hpp"
#include "cppa/attachable.hpp"
#include "cppa/message_id.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/util/rm_ref.hpp"

namespace cppa {

class actor;
class self_type;
class serializer;
class deserializer;

/**
 * @brief A unique actor ID.
 * @relates actor
 */
typedef std::uint32_t actor_id;

/**
 * @brief A smart pointer type that manages instances of {@link actor}.
 * @relates actor
 */
typedef intrusive_ptr<actor> actor_ptr;

/**
 * @brief Base class for all actor implementations.
 */
class actor : public channel {

 public:

    /**
     * @brief Enqueues @p msg to the actor's mailbox and returns true if
     *        this actor is an scheduled actor that successfully changed
     *        its state to @p pending in response to the enqueue operation.
     */
    virtual bool chained_enqueue(const actor_ptr& sender, any_tuple msg);

    /**
     * @brief Enqueues @p msg as a synchronous message to this actor's mailbox.
     * @pre <tt>id.valid()</tt>
     */
    virtual void sync_enqueue(const actor_ptr& sender,
                              message_id_t id,
                              any_tuple msg) = 0;

    /**
     * @brief Enqueues @p msg as a reply to @p request_id
     *        to this actor's mailbox and returns true if
     *        this actor is an scheduled actor that successfully changed
     *        its state to @p pending in response to the enqueue operation.
     */
    virtual bool chained_sync_enqueue(const actor_ptr& sender,
                                      message_id_t id,
                                      any_tuple msg);

    /**
     * @brief Attaches @p ptr to this actor.
     *
     * The actor will call <tt>ptr->detach(...)</tt> on exit, or immediately
     * if it already finished execution.
     * @param ptr A callback object that's executed if the actor finishes
     *            execution.
     * @returns @c true if @p ptr was successfully attached to the actor;
     *          otherwise (actor already exited) @p false.
     * @warning The actor takes ownership of @p ptr.
     */
    virtual bool attach(attachable_ptr ptr);

    /**
     * @brief Convenience function that attaches the functor
     *        or function @p f to this actor.
     *
     * The actor executes <tt>f()</tt> on exit, or immediatley
     * if it already finished execution.
     * @param f A functor, function or lambda expression that's executed
     *             if the actor finishes execution.
     * @returns @c true if @p f was successfully attached to the actor;
     *          otherwise (actor already exited) @p false.
     */
    template<typename F>
    bool attach_functor(F&& f);

    /**
     * @brief Detaches the first attached object that matches @p what.
     */
    virtual void detach(const attachable::token& what);

    /**
     * @brief Links this actor to @p other.
     * @param other Actor instance that whose execution is coupled to the
     *              execution of this Actor.
     */
    virtual void link_to(const actor_ptr& other);

    /**
     * @brief Unlinks this actor from @p other.
     * @param other Linked Actor.
     * @note Links are automatically removed if the actor finishes execution.
     */
    virtual void unlink_from(const actor_ptr& other);

    /**
     * @brief Establishes a link relation between this actor and @p other.
     * @param other Actor instance that wants to link against this Actor.
     * @returns @c true if this actor is running and added @p other to its
     *          list of linked actors; otherwise @c false.
     */
    virtual bool establish_backlink(const actor_ptr& other);

    /**
     * @brief Removes a link relation between this actor and @p other.
     * @param other Actor instance that wants to unlink from this Actor.
     * @returns @c true if this actor is running and removed @p other
     *          from its list of linked actors; otherwise @c false.
     */
    virtual bool remove_backlink(const actor_ptr& other);

    /**
     * @brief Gets an integer value that uniquely identifies this Actor in
     *        the process it's executed in.
     * @returns The unique identifier of this actor.
     */
    inline actor_id id() const;

    /**
     * @brief Checks if this actor is running on a remote node.
     * @returns @c true if this actor represents a remote actor;
     *          otherwise @c false.
     */
    inline bool is_proxy() const;


 protected:

    actor();

    actor(actor_id aid);

    /**
     * @brief Should be overridden by subtypes and called upon termination.
     * @note Default implementation sets 'exit_reason' accordingly.
     * @note Overridden functions should always call super::cleanup().
     */
    virtual void cleanup(std::uint32_t reason);

    /**
     * @brief The default implementation for {@link link_to()}.
     */
    bool link_to_impl(const actor_ptr& other);

    /**
     * @brief The default implementation for {@link unlink_from()}.
     */
    bool unlink_from_impl(const actor_ptr& other);

    /**
     * @brief Returns the actor's exit reason of
     *        <tt>exit_reason::not_exited</tt> if it's still alive.
     */
    inline std::uint32_t exit_reason() const;

    /**
     * @brief Returns <tt>exit_reason() != exit_reason::not_exited</tt>.
     */
    inline bool exited() const;

 private:

    // cannot be changed after construction
    const actor_id m_id;

    // you're either a proxy or you're not
    const bool m_is_proxy;

    // initially exit_reason::not_exited
    std::atomic<std::uint32_t> m_exit_reason;

    // guards access to m_exit_reason, m_attachables, and m_links
    std::mutex m_mtx;

    // links to other actors
    std::vector<actor_ptr> m_links;

    // attached functors that are executed on cleanup
    std::vector<attachable_ptr> m_attachables;

};

// undocumented, because self_type is hidden in documentation
bool operator==(const actor_ptr&, const self_type&);
bool operator!=(const self_type&, const actor_ptr&);

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

inline std::uint32_t actor::id() const {
    return m_id;
}

inline bool actor::is_proxy() const {
    return m_is_proxy;
}

inline std::uint32_t actor::exit_reason() const {
    return m_exit_reason;
}

inline bool actor::exited() const {
    return exit_reason() != exit_reason::not_exited;
}

template<class F>
struct functor_attachable : attachable {

    F m_functor;

    template<typename T>
    inline functor_attachable(T&& arg) : m_functor(std::forward<T>(arg)) { }

    void actor_exited(std::uint32_t reason) { m_functor(reason); }

    bool matches(const attachable::token&) { return false; }

};

template<typename F>
bool actor::attach_functor(F&& f) {
    typedef typename util::rm_ref<F>::type f_type;
    typedef functor_attachable<f_type> impl;
    return attach(attachable_ptr{new impl(std::forward<F>(f))});
}

} // namespace cppa

#endif // CPPA_ACTOR_HPP
