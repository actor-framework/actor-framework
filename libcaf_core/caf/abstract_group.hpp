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

#ifndef CAF_ABSTRACT_GROUP_HPP
#define CAF_ABSTRACT_GROUP_HPP

#include <string>
#include <memory>

#include "caf/channel.hpp"
#include "caf/attachable.hpp"
#include "caf/ref_counted.hpp"
#include "caf/abstract_channel.hpp"

namespace caf {
namespace detail {

class group_manager;
class peer_connection;

} // namespace detail
} // namespace caf

namespace caf {

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

    using module_ptr = module*;
    using unique_module_ptr = std::unique_ptr<module>;

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
using abstract_group_ptr = intrusive_ptr<abstract_group>;

} // namespace caf

#endif // CAF_ABSTRACT_GROUP_HPP
