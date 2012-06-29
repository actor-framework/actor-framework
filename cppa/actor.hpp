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


#ifndef CPPA_ACTOR_HPP
#define CPPA_ACTOR_HPP

#include <memory>
#include <cstdint>
#include <type_traits>

#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/attachable.hpp"
#include "cppa/process_information.hpp"

#include "cppa/util/rm_ref.hpp"

namespace cppa {

class serializer;
class deserializer;

class actor;

/**
 * @brief A unique actor ID.
 * @relates actor
 */
typedef std::uint32_t actor_id;

/**
 * @brief Base class for all actor implementations.
 */
class actor : public channel {

 public:

    /**
     * @brief Enqueues @p msg to the actor's mailbox and returns true if
     *        this actor is an scheduled actor that successfully changed
     *        its state to @p pending.
     */
    virtual bool chained_enqueue(actor* sender, any_tuple msg);

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
    virtual bool attach(attachable* ptr) = 0;

    /**
     * @brief Convenience function that attaches the functor
     *        or function @p ftor to this actor.
     *
     * The actor executes <tt>ftor()</tt> on exit, or immediatley
     * if it already finished execution.
     * @param ftor A functor, function or lambda expression that's executed
     *             if the actor finishes execution.
     * @returns @c true if @p ftor was successfully attached to the actor;
     *          otherwise (actor already exited) @p false.
     */
    template<typename F>
    bool attach_functor(F&& ftor);

    /**
     * @brief Detaches the first attached object that matches @p what.
     */
    virtual void detach(const attachable::token& what) = 0;

    template<typename T>
    bool attach(std::unique_ptr<T>&& ptr,
                typename std::enable_if<
                    std::is_base_of<attachable,T>::value
                >::type* = 0);

    /**
     * @brief Links this actor to @p other.
     * @param other Actor instance that whose execution is coupled to the
     *              execution of this Actor.
     */
    virtual void link_to(intrusive_ptr<actor>& other) = 0;

    /**
     * @brief Unlinks this actor from @p other.
     * @param other Linked Actor.
     * @note Links are automatically removed if the Actor finishes execution.
     */
    virtual void unlink_from(intrusive_ptr<actor>& other) = 0;

    /**
     * @brief Establishes a link relation between this actor and @p other.
     * @param other Actor instance that wants to link against this Actor.
     * @returns @c true if this actor is running and added @p other to its
     *          list of linked actors; otherwise @c false.
     */
    virtual bool establish_backlink(intrusive_ptr<actor>& other) = 0;

    /**
     * @brief Removes a link relation between this actor and @p other.
     * @param other Actor instance that wants to unlink from this Actor.
     * @returns @c true if this actor is running and removed @p other
     *          from its list of linked actors; otherwise @c false.
     */
    virtual bool remove_backlink(intrusive_ptr<actor>& other) = 0;

    // rvalue support
    /**
     * @copydoc link_to(intrusive_ptr<actor>&)
     */
    void link_to(intrusive_ptr<actor>&& other);

    /**
     * @copydoc :unlink_from(intrusive_ptr<actor>&)
     */
    void unlink_from(intrusive_ptr<actor>&& other);

    /**
     * @copydoc remove_backlink(intrusive_ptr<actor>&)
     */
    bool remove_backlink(intrusive_ptr<actor>&& other);

    /**
     * @copydoc establish_backlink(intrusive_ptr<actor>&)
     */
    bool establish_backlink(intrusive_ptr<actor>&& other);

    /**
     * @brief Gets the {@link process_information} of the parent process.
     * @returns The {@link process_information} of the parent process.
     */
    inline const process_information& parent_process() const;

    /**
     * @brief Gets the {@link process_information} pointer
     *        of the parent process.
     * @returns A pointer to the {@link process_information}
     *          of the parent process.
     */
    inline process_information_ptr parent_process_ptr() const;

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

    actor(const process_information_ptr& parent = process_information::get());

    actor(std::uint32_t aid,
          const process_information_ptr& parent = process_information::get());

 private:

    actor_id m_id;
    bool m_is_proxy;
    process_information_ptr m_parent_process;

};

/**
 * @brief A smart pointer type that manages instances of {@link actor}.
 * @relates actor
 */
typedef intrusive_ptr<actor> actor_ptr;

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

inline const process_information& actor::parent_process() const {
    return *m_parent_process;
}

inline process_information_ptr actor::parent_process_ptr() const {
    return m_parent_process;
}

inline std::uint32_t actor::id() const {
    return m_id;
}

inline bool actor::is_proxy() const {
    return m_is_proxy;
}

template<typename T>
bool actor::attach(std::unique_ptr<T>&& ptr,
                   typename std::enable_if<
                       std::is_base_of<attachable,T>::value
                   >::type*) {
    return attach(static_cast<attachable*>(ptr.release()));
}

template<class F>
class functor_attachable : public attachable {

    F m_functor;

 public:

    template<class FArg>
    functor_attachable(FArg&& arg) : m_functor(std::forward<FArg>(arg)) {
    }

    void actor_exited(std::uint32_t reason) {
        m_functor(reason);
    }

    bool matches(const attachable::token&) {
        return false;
    }

};

template<typename F>
bool actor::attach_functor(F&& ftor) {
    typedef typename util::rm_ref<F>::type f_type;
    return attach(new functor_attachable<f_type>(std::forward<F>(ftor)));
}

} // namespace cppa

#endif // CPPA_ACTOR_HPP
