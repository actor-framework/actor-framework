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


#ifndef CPPA_ACTOR_PROXY_CACHE_HPP
#define CPPA_ACTOR_PROXY_CACHE_HPP

#include <mutex>
#include <thread>
#include <string>
#include <limits>
#include <vector>
#include <functional>

#include "cppa/actor_proxy.hpp"
#include "cppa/process_information.hpp"

#include "cppa/util/shared_spinlock.hpp"

namespace cppa { namespace detail {

class actor_proxy_cache {

 public:

    actor_proxy_ptr get(actor_id aid, std::uint32_t process_id,
                        const process_information::node_id_type& node_id);

    // @returns true if pptr was successfully removed, false otherwise
    bool erase(const actor_proxy_ptr& pptr);

    template<typename Fun>
    void erase_all(const process_information::node_id_type& nid,
                   std::uint32_t process_id,
                   Fun fun) {
        key_tuple lb{nid, process_id, std::numeric_limits<actor_id>::min()};
        key_tuple ub{nid, process_id, std::numeric_limits<actor_id>::max()};
        { // lifetime scope of guard
            std::lock_guard<util::shared_spinlock> guard(m_lock);
            auto e = m_entries.end();
            auto first = m_entries.lower_bound(lb);
            if (first != e) {
                auto last = m_entries.upper_bound(ub);
                for (auto i = first; i != last; ++i) {
                    fun(i->second);
                }
                m_entries.erase(first, last);
            }
        }
    }

 private:

    typedef std::tuple<process_information::node_id_type, // node id
                       std::uint32_t,                     // process id
                       actor_id>                          // (remote) actor id
            key_tuple;

    struct key_tuple_less {
        bool operator()(const key_tuple& lhs, const key_tuple& rhs) const;
    };

    util::shared_spinlock m_lock;
    std::map<key_tuple, actor_proxy_ptr, key_tuple_less> m_entries;

    actor_proxy_ptr get_impl(const key_tuple& key);


};

// get the thread-local cache object
actor_proxy_cache& get_actor_proxy_cache();

} } // namespace cppa::detail

#endif // CPPA_ACTOR_PROXY_CACHE_HPP
