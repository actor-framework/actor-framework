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
 * Copyright (C) 2011-2014                                                    *
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
