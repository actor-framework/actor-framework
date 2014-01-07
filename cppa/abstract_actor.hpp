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


#ifndef CPPA_ABSTRACT_ACTOR_HPP
#define CPPA_ABSTRACT_ACTOR_HPP

#include <mutex>
#include <atomic>
#include <memory>
#include <vector>
#include <cstdint>
#include <type_traits>

#include "cppa/node_id.hpp"
#include "cppa/cppa_fwd.hpp"
#include "cppa/attachable.hpp"
#include "cppa/message_id.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/abstract_channel.hpp"

#include "cppa/util/type_traits.hpp"

namespace cppa {

class actor_addr;
class serializer;
class deserializer;

/**
 * @brief A unique actor ID.
 * @relates actor
 */
typedef std::uint32_t actor_id;

class abstract_actor;

typedef intrusive_ptr<abstract_actor> abstract_actor_ptr;

/**
 * @brief Base class for all actor implementations.
 */
class abstract_actor : public abstract_channel {

 public:

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
    bool attach(attachable_ptr ptr);

    /**
     * @brief Returns the address of this actor.
     */
    actor_addr address() const;

    /**
     * @brief Detaches the first attached object that matches @p what.
     */
    void detach(const attachable::token& what);

    /**
     * @brief Links this actor to @p other.
     * @param other Actor instance that whose execution is coupled to the
     *              execution of this Actor.
     */
    virtual void link_to(const actor_addr& other);

    /**
     * @brief Unlinks this actor from @p other.
     * @param other Linked Actor.
     * @note Links are automatically removed if the actor finishes execution.
     */
    virtual void unlink_from(const actor_addr& other);

    /**
     * @brief Establishes a link relation between this actor and @p other.
     * @param other Actor instance that wants to link against this Actor.
     * @returns @c true if this actor is running and added @p other to its
     *          list of linked actors; otherwise @c false.
     */
    virtual bool establish_backlink(const actor_addr& other);

    /**
     * @brief Removes a link relation between this actor and @p other.
     * @param other Actor instance that wants to unlink from this Actor.
     * @returns @c true if this actor is running and removed @p other
     *          from its list of linked actors; otherwise @c false.
     */
    virtual bool remove_backlink(const actor_addr& other);

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

    /**
     * @brief Returns the ID of the node this actor is running on.
     */
    inline const node_id& node() const;

    /**
     * @brief Returns the actor's exit reason of
     *        <tt>exit_reason::not_exited</tt> if it's still alive.
     */
    inline std::uint32_t exit_reason() const;

 protected:

    abstract_actor();

    abstract_actor(actor_id aid);

    /**
     * @brief Should be overridden by subtypes and called upon termination.
     * @note Default implementation sets 'exit_reason' accordingly.
     * @note Overridden functions should always call super::cleanup().
     */
    virtual void cleanup(std::uint32_t reason);

    /**
     * @brief The default implementation for {@link link_to()}.
     */
    bool link_to_impl(const actor_addr& other);

    /**
     * @brief The default implementation for {@link unlink_from()}.
     */
    bool unlink_from_impl(const actor_addr& other);

    /**
     * @brief Returns <tt>exit_reason() != exit_reason::not_exited</tt>.
     */
    inline bool exited() const;

    // cannot be changed after construction
    const actor_id m_id;

    // you're either a proxy or you're not
    const bool m_is_proxy;

 private:

    // initially exit_reason::not_exited
    std::atomic<std::uint32_t> m_exit_reason;

    // guards access to m_exit_reason, m_attachables, and m_links
    std::mutex m_mtx;

    // links to other actors
    std::vector<abstract_actor_ptr> m_links;

    // attached functors that are executed on cleanup
    std::vector<attachable_ptr> m_attachables;

 protected:

    // identifies the node this actor is running on
    node_id_ptr m_node;

};

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

inline std::uint32_t abstract_actor::id() const {
    return m_id;
}

inline bool abstract_actor::is_proxy() const {
    return m_is_proxy;
}

inline std::uint32_t abstract_actor::exit_reason() const {
    return m_exit_reason;
}

inline bool abstract_actor::exited() const {
    return exit_reason() != exit_reason::not_exited;
}

inline const node_id& abstract_actor::node() const {
    return *m_node;
}

} // namespace cppa

#endif // CPPA_ABSTRACT_ACTOR_HPP
