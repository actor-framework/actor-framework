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


#ifndef CPPA_GROUP_HPP
#define CPPA_GROUP_HPP

#include <string>
#include <memory>

#include "cppa/channel.hpp"
#include "cppa/attachable.hpp"
#include "cppa/ref_counted.hpp"

namespace cppa { namespace detail { class group_manager; } }

namespace cppa {

/**
 * @brief A multicast group.
 */
class group : public channel {

    friend class detail::group_manager;

    std::string m_identifier;
    std::string m_module_name;

 protected:

    group(std::string&& id, std::string&& mod_name);

    group(const std::string& id, const std::string& mod_name);

    virtual void unsubscribe(const channel_ptr& who) = 0;

 public:

    class unsubscriber;

    // needs access to unsubscribe()
    friend class unsubscriber;

    // unsubscribes its channel from the group on destruction
    class unsubscriber : public attachable {

        friend class group;

        channel_ptr m_self;
        intrusive_ptr<group> m_group;

     public:

        unsubscriber(const channel_ptr& s, const intrusive_ptr<group>& g);

        ~unsubscriber();

        void actor_exited(std::uint32_t);

        // matches on m_group
        bool matches(const attachable::token& what);

    };

    typedef std::unique_ptr<unsubscriber> subscription;

    /**
     * @brief Module interface.
     */
    class module {

        std::string m_name;

     protected:

        module(std::string module_name);

     public:

        /**
         * @brief Get the name of this module implementation.
         * @returns The name of this module implementation.
         * @threadsafe
         */
        const std::string& name();

        /**
         * @brief Get a pointer to the group associated with
         *        the name @p group_name.
         * @threadsafe
         */
        virtual intrusive_ptr<group> get(const std::string& group_name) = 0;

    };

    /**
     * @brief A string representation of the group identifier.
     * @returns The group identifier as string (e.g. "224.0.0.1" for IPv4
     *         multicast or a user-defined string for local groups).
     */
    const std::string& identifier() const;

    /**
     * @brief The name of the module.
     * @returns The module name of this group (e.g. "local").
     */
    const std::string& module_name() const;

    /**
     * @brief Subscribe @p who to this group.
     * @returns A {@link subscription} object that unsubscribes @p who
     *         if the lifetime of @p who ends.
     */
    virtual subscription subscribe(const channel_ptr& who) = 0;

    /**
     * @brief Get a pointer to the group associated with
     *        @p group_identifier from the module @p module_name.
     * @threadsafe
     */
    static intrusive_ptr<group> get(const std::string& module_name,
                                    const std::string& group_identifier);

    /**
     * @brief Returns an anonymous group.
     *
     * Each calls to this member function returns a new instances
     * of an anonymous group. Anonymous groups can be used whenever
     * a set of actors wants to communicate over an exclusive channel.
     */
    static intrusive_ptr<group> anonymous();

    /**
     * @brief Add a new group module to the libcppa group management.
     * @threadsafe
     */
    static void add_module(module*);

};

/**
 * @brief A smart pointer type that manages instances of {@link group}.
 * @relates group
 */
typedef intrusive_ptr<group> group_ptr;

} // namespace cppa

#endif // CPPA_GROUP_HPP
