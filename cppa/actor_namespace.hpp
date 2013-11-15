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


#ifndef CPPA_ACTOR_NAMESPACE_HPP
#define CPPA_ACTOR_NAMESPACE_HPP

#include <map>
#include <utility>
#include <functional>

#include "cppa/node_id.hpp"
#include "cppa/actor_proxy.hpp"

namespace cppa {

class serializer;
class deserializer;

namespace io { class middleman; }

/**
 * @brief Groups a (distributed) set of actors and allows actors
 *        in the same namespace to exchange messages.
 */
class actor_namespace {

 public:
    
    typedef std::function<actor_proxy_ptr(actor_id, node_id_ptr)>
            factory_fun;

    typedef std::function<void(actor_id, const node_id&)>
            new_element_callback;

    inline void set_proxy_factory(factory_fun fun);
    
    inline void set_new_element_callback(new_element_callback fun);
    
    void write(serializer* sink, const actor_addr& ptr);
    
    actor_addr read(deserializer* source);

    /**
     * @brief A map that stores weak actor proxy pointers by actor ids.
     */
    typedef std::map<actor_id, weak_actor_proxy_ptr> proxy_map;
    
    /**
     * @brief Returns the number of proxies for @p node.
     */
    size_t count_proxies(const node_id& node);
    
    /**
     * @brief Returns the proxy instance identified by @p node and @p aid
     *        or @p nullptr if the actor is unknown.
     */
    actor_proxy_ptr get(const node_id& node, actor_id aid);

    /**
     * @brief Returns the proxy instance identified by @p node and @p aid
     *        or creates a new (default) proxy instance.
     */
    actor_proxy_ptr get_or_put(node_id_ptr node, actor_id aid);
    
    /**
     * @brief Stores @p proxy in the list of known actor proxies.
     */
    void put(const node_id& parent,
             actor_id aid,
             const actor_proxy_ptr& proxy);
    
    /**
     * @brief Returns the map of known actors for @p node.
     */
    proxy_map& proxies(node_id& node);
    
    /**
     * @brief Deletes all proxies for @p node.
     */
    void erase(node_id& node);
    
    /**
     * @brief Deletes the proxy with id @p aid for @p node.
     */
    void erase(node_id& node, actor_id aid);
    
 private:
    
    factory_fun m_factory;
    
    new_element_callback m_new_element_callback;
    
    node_id_ptr m_node;
    
    std::map<node_id, proxy_map> m_proxies;
    
};

inline void actor_namespace::set_proxy_factory(factory_fun fun) {
    m_factory = std::move(fun);
}
    
inline void actor_namespace::set_new_element_callback(new_element_callback fun) {
    m_new_element_callback = std::move(fun);
}
    
} // namespace cppa

#endif
