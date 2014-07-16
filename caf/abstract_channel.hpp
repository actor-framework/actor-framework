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

#ifndef CAF_ABSTRACT_CHANNEL_HPP
#define CAF_ABSTRACT_CHANNEL_HPP

#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/message_id.hpp"
#include "caf/ref_counted.hpp"

namespace caf {

/**
 * @brief Interface for all message receivers.
 *
 * This interface describes an entity that can receive messages
 * and is implemented by {@link actor} and {@link group}.
 */
class abstract_channel : public ref_counted {

 public:

    virtual ~abstract_channel();

    /**
     * @brief Enqueues a new message to the channel.
     * @param header Contains meta information about this message
     *               such as the address of the sender and the
     *               ID of the message if it is a synchronous message.
     * @param content The content encapsulated in a copy-on-write tuple.
     * @param host Pointer to the {@link execution_unit execution unit} the
     *             caller is executed by or @p nullptr if the caller
     *             is not a scheduled actor.
     */
    virtual void enqueue(const actor_addr& sender, message_id mid,
                         message content, execution_unit* host) = 0;

    /**
     * @brief Returns the ID of the node this actor is running on.
     */
    inline node_id node() const;

    /**
     * @brief Returns true if {@link node_ptr} returns
     */
    bool is_remote() const;

 protected:

    abstract_channel();

    abstract_channel(node_id nid);

 private:

    // identifies the node of this channel
    node_id m_node;

};

using abstract_channel_ptr = intrusive_ptr<abstract_channel>;

inline node_id abstract_channel::node() const {
    return m_node;
}

} // namespace caf

#endif // CAF_ABSTRACT_CHANNEL_HPP
