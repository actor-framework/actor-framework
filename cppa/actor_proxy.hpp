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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_ACTOR_PROXY_HPP
#define CPPA_ACTOR_PROXY_HPP

#include "cppa/extend.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/message_header.hpp"
#include "cppa/enable_weak_ptr.hpp"
#include "cppa/weak_intrusive_ptr.hpp"

namespace cppa {

class actor_proxy_cache;

/**
 * @brief Represents a remote actor.
 * @extends abstract_actor
 */
class actor_proxy : public extend<abstract_actor>::with<enable_weak_ptr> {

    typedef combined_type super;

 public:

    ~actor_proxy();

    /**
     * @brief Establishes a local link state that's not synchronized back
     *        to the remote instance.
     */
    virtual void local_link_to(const actor_addr& other) = 0;

    /**
     * @brief Removes a local link state.
     */
    virtual void local_unlink_from(const actor_addr& other) = 0;

    /**
     * @brief Delivers given message via this proxy instance.
     *
     * This function is meant to give the proxy the opportunity to keep track
     * of synchronous communication or perform other bookkeeping if needed.
     * The member function is called by the protocol from inside the
     * middleman's thread.
     * @note This function is guaranteed to be called non-concurrently.
     */
    virtual void deliver(msg_hdr_cref hdr, message msg) = 0;

 protected:

    actor_proxy(actor_id mid);

};

/**
 * @brief A smart pointer to an {@link actor_proxy} instance.
 * @relates actor_proxy
 */
typedef intrusive_ptr<actor_proxy> actor_proxy_ptr;

/**
 * @brief A weak smart pointer to an {@link actor_proxy} instance.
 * @relates actor_proxy
 */
typedef weak_intrusive_ptr<actor_proxy> weak_actor_proxy_ptr;

} // namespace cppa

#endif // CPPA_ACTOR_PROXY_HPP
