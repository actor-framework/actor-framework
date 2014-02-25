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


#ifndef CPPA_ACTOR_PROXY_CACHE_HPP
#define CPPA_ACTOR_PROXY_CACHE_HPP

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/ref_counted.hpp"

namespace cppa {

class serializer;
class deserializer;

/**
 * @brief Different serialization protocols have different representations
 *        for actors. This class encapsulates a technology-specific
 *        actor addressing.
 */
class actor_addressing {

 public:

    virtual ~actor_addressing();

    /**
     * @brief Returns the technology identifier of the implementation.
     * @note All-uppercase identifiers are reserved for libcppa's
     *       builtin implementations.
     */
    virtual atom_value technology_id() const = 0;

    /**
     * @brief Serializes @p ptr to @p sink according
     *        to the implemented addressing.
     * @warning Not thread-safe
     */
    virtual void write(serializer* sink, const actor_ptr& ptr) = 0;

    /**
     * @brief Deserializes an actor from @p source according
     *        to the implemented addressing.
     * @warning Not thread-safe
     */
     virtual actor_ptr read(deserializer* source) = 0;

};

} // namespace cppa::detail

#endif // CPPA_ACTOR_PROXY_CACHE_HPP
