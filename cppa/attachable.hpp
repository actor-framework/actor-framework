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


#ifndef CPPA_ATTACHABLE_HPP
#define CPPA_ATTACHABLE_HPP

#include <cstdint>
#include <typeinfo>

namespace cppa {

/**
 * @brief Callback utility class.
 */
class attachable {

    attachable(const attachable&) = delete;
    attachable& operator=(const attachable&) = delete;

 protected:

    attachable() = default;

 public:

    /**
     * @brief Represents a pointer to a value with its RTTI.
     */
    struct token {
        /**
         * @brief Denotes the type of @c ptr.
         */
        const std::type_info& subtype;
        /**
         * @brief Any value, used to identify @c attachable instances.
         */
        const void* ptr;
        inline token(const std::type_info& msubtype, const void* mptr)
            : subtype(msubtype), ptr(mptr) {
        }
    };

    virtual ~attachable();

    /**
     * @brief Executed if the actor finished execution with given @p reason.
     *
     * The default implementation does nothing.
     * @param reason The exit rason of the observed actor.
     */
    virtual void actor_exited(std::uint32_t reason) = 0;

    /**
     * @brief Selects a group of @c attachable instances by @p what.
     * @param what A value that selects zero or more @c attachable instances.
     * @returns @c true if @p what selects this instance; otherwise @c false.
     */
    virtual bool matches(const token& what) = 0;

};

} // namespace cppa

#endif // CPPA_ATTACHABLE_HPP
