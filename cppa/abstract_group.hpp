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


#ifndef CPPA_ABSTRACT_GROUP_HPP
#define CPPA_ABSTRACT_GROUP_HPP

#include <string>
#include <memory>

#include "cppa/channel.hpp"
#include "cppa/attachable.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/abstract_channel.hpp"

namespace cppa { namespace detail {

class group_manager;
class peer_connection;

} } // namespace cppa::detail

namespace cppa {

class group;
class serializer;
class deserializer;

/**
 * @brief A multicast group.
 */
class abstract_group : public abstract_channel {

    friend class detail::group_manager;
    friend class detail::peer_connection; // needs access to remote_enqueue

 public:

    ~abstract_group();

    class subscription;

    // needs access to unsubscribe()
    friend class subscription;

    // unsubscribes its channel from the group on destruction
    class subscription {

        friend class abstract_group;

        subscription(const subscription&) = delete;
        subscription& operator=(const subscription&) = delete;

     public:

        subscription() = default;
        subscription(subscription&&) = default;
        subscription(const channel& s, const intrusive_ptr<abstract_group>& g);

        ~subscription();

        inline bool valid() const { return (m_subscriber) && (m_group); }

     private:

        channel m_subscriber;
        intrusive_ptr<abstract_group> m_group;

    };

    /**
     * @brief Module interface.
     */
    class module {

        std::string m_name;

     protected:

        module(std::string module_name);

     public:

        virtual ~module();

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
        virtual group get(const std::string& group_name) = 0;

        virtual group deserialize(deserializer* source) = 0;

    };

    typedef module* module_ptr;
    typedef std::unique_ptr<module> unique_module_ptr;

    virtual void serialize(serializer* sink) = 0;

    /**
     * @brief A string representation of the group identifier.
     * @returns The group identifier as string (e.g. "224.0.0.1" for IPv4
     *         multicast or a user-defined string for local groups).
     */
    const std::string& identifier() const;

    module_ptr get_module() const;

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
    virtual subscription subscribe(const channel& who) = 0;

 protected:

    abstract_group(module_ptr module, std::string group_id);

    virtual void unsubscribe(const channel& who) = 0;

    module_ptr m_module;
    std::string m_identifier;

};

/**
 * @brief A smart pointer type that manages instances of {@link group}.
 * @relates group
 */
typedef intrusive_ptr<abstract_group> abstract_group_ptr;

/**
 * @brief Makes *all* local groups accessible via network on address @p addr
 *        and @p port.
 * @throws bind_failure
 * @throws network_error
 */
void publish_local_groups(std::uint16_t port, const char* addr = nullptr);

} // namespace cppa

#endif // CPPA_ABSTRACT_GROUP_HPP
