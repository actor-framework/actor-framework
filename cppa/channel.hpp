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


#ifndef CPPA_CHANNEL_HPP
#define CPPA_CHANNEL_HPP

#include <type_traits>

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {

// forward declarations
class actor;
class group;
class any_tuple;

typedef intrusive_ptr<actor> actor_ptr;

/**
 * @brief Interface for all message receivers.
 *
 * This interface describes an entity that can receive messages
 * and is implemented by {@link actor} and {@link group}.
 */
class channel : public ref_counted {

    friend class actor;
    friend class group;

 public:

    /**
     * @brief Enqueues @p msg to the list of received messages.
     */
    virtual void enqueue(const actor_ptr& sender, any_tuple msg) = 0;

 protected:

    virtual ~channel();

};

/**
 * @brief A smart pointer type that manages instances of {@link channel}.
 * @relates channel
 */
typedef intrusive_ptr<channel> channel_ptr;

/**
 * @brief Convenience alias.
 */
template<typename T, typename R = void>
using enable_if_channel = std::enable_if<std::is_base_of<channel,T>::value,R>;

} // namespace cppa

#endif // CPPA_CHANNEL_HPP
