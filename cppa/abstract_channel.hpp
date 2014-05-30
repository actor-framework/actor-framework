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


#ifndef CPPA_ABSTRACT_CHANNEL_HPP
#define CPPA_ABSTRACT_CHANNEL_HPP

#include "cppa/cppa_fwd.hpp"
#include "cppa/ref_counted.hpp"

namespace cppa {

/**
 * @brief Interface for all message receivers.
 *
 * This interface describes an entity that can receive messages
 * and is implemented by {@link actor} and {@link group}.
 */
class abstract_channel : public ref_counted {

 public:

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
    virtual void enqueue(msg_hdr_cref header,
                         any_tuple content,
                         execution_unit* host) = 0;

 protected:

    virtual ~abstract_channel();

};

} // namespace cppa

#endif // CPPA_ABSTRACT_CHANNEL_HPP
