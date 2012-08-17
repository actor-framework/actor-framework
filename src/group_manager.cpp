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


#include <set>
#include <stdexcept>

#include "cppa/cppa.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/event_based_actor.hpp"

#include "cppa/detail/middleman.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/group_manager.hpp"
#include "cppa/detail/addressed_message.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

namespace {

using namespace cppa;

typedef std::lock_guard<util::shared_spinlock> exclusive_guard;
typedef util::shared_lock_guard<util::shared_spinlock> shared_guard;
typedef util::upgrade_lock_guard<util::shared_spinlock> upgrade_guard;

class local_broker;
class local_group_module;

class local_group : public group {

 public:

    void send_all_subscribers(actor* sender, const any_tuple& msg) {
        shared_guard guard(m_shared_mtx);
        for (auto& s : m_subscribers) {
            s->enqueue(sender, msg);
        }
    }

    void enqueue(actor* sender, any_tuple msg) {
        send_all_subscribers(sender, msg);
        m_broker->enqueue(sender, std::move(msg));
    }

    std::pair<bool, size_t> add_subscriber(const channel_ptr& who) {
        exclusive_guard guard(m_shared_mtx);
        if (m_subscribers.insert(who).second) {
            return {true, m_subscribers.size()};
        }
        return {false, m_subscribers.size()};
    }

    std::pair<bool, size_t> erase_subscriber(const channel_ptr& who) {
        exclusive_guard guard(m_shared_mtx);
        auto erased_one = m_subscribers.erase(who) > 0;
        return {erased_one, m_subscribers.size()};
    }

    group::subscription subscribe(const channel_ptr& who) {
        if (add_subscriber(who).first) {
            return {who, this};
        }
        return {};
    }

    void unsubscribe(const channel_ptr& who) {
        erase_subscriber(who);
    }

    void serialize(serializer* sink);

    inline const process_information& process() const {
        return *m_process;
    }

    inline const process_information_ptr& process_ptr() const {
        return m_process;
    }

    const actor_ptr& broker() const {
        return m_broker;
    }

    local_group(bool spawn_local_broker,
                local_group_module* mod, std::string id,
                process_information_ptr parent = process_information::get());

 protected:

    process_information_ptr m_process;
    util::shared_spinlock m_shared_mtx;
    std::set<channel_ptr> m_subscribers;
    actor_ptr m_broker;

};

typedef intrusive_ptr<local_group> local_group_ptr;

class local_broker : public event_based_actor {

 public:

    local_broker(local_group_ptr g) : m_group(std::move(g)) { }

    void init() {
        become (
            on(atom("JOIN"), arg_match) >> [=](const actor_ptr& other) {
                if (other && m_acquaintances.insert(other).second) {
                    monitor(other);
                }
            },
            on(atom("LEAVE"), arg_match) >> [=](const actor_ptr& other) {
                if (other && m_acquaintances.erase(other) > 0) {
                    demonitor(other);
                }
            },
            on(atom("FORWARD"), arg_match) >> [=](const any_tuple& what) {
                m_group->send_all_subscribers(last_sender().get(), what);
            },
            on<atom("DOWN"), std::uint32_t>() >> [=] {
                actor_ptr other = last_sender();
                if (other) m_acquaintances.erase(other);
            },
            others() >> [=] {
                auto sender = last_sender().get();
                for (auto& acquaintance : m_acquaintances) {
                    acquaintance->enqueue(sender, last_dequeued());
                }
            }
        );
    }

 private:

    local_group_ptr m_group;
    std::set<actor_ptr> m_acquaintances;

};

// Send a "JOIN" message to the original group if a proxy
// has local subscriptions and a "LEAVE" message to the original group
// if there's no subscription left.

class proxy_broker;

class local_group_proxy : public local_group {

    typedef local_group super;

 public:

    template<typename... Args>
    local_group_proxy(actor_ptr remote_broker, Args&&... args)
    : super(false, std::forward<Args>(args)...) {
        CPPA_REQUIRE(m_broker == nullptr);
        CPPA_REQUIRE(remote_broker != nullptr);
        CPPA_REQUIRE(remote_broker->is_proxy());
        m_broker = std::move(remote_broker);
        m_proxy_broker = spawn_hidden<proxy_broker>(this);
    }

    group::subscription subscribe(const channel_ptr& who) {
        auto res = add_subscriber(who);
        if (res.first) {
            if (res.second == 1) {
                // join the remote source
                m_broker->enqueue(nullptr,
                                  make_any_tuple(atom("JOIN"), m_proxy_broker));
            }
            return {who, this};
        }
        return {};
    }

    void unsubscribe(const channel_ptr& who) {
        auto res = erase_subscriber(who);
        if (res.first && res.second == 0) {
            // leave the remote source,
            // because there's no more subscriber on this node
            m_broker->enqueue(nullptr,
                              make_any_tuple(atom("LEAVE"), m_proxy_broker));
        }
    }

    void enqueue(actor* sender, any_tuple msg) {
        // forward message to the broker
        m_broker->enqueue(sender,
                          make_any_tuple(atom("FORWARD"), std::move(msg)));
    }

 private:

    actor_ptr m_proxy_broker;

};

typedef intrusive_ptr<local_group_proxy> local_group_proxy_ptr;

class proxy_broker : public event_based_actor {

 public:

    proxy_broker(local_group_proxy_ptr grp) : m_group(std::move(grp)) { }

    void init() {
        become (
            others() >> [=] {
                m_group->send_all_subscribers(last_sender().get(),
                                              last_dequeued());
            }
        );
    }

 private:

    local_group_proxy_ptr m_group;

};

class local_group_module : public group::module {

    typedef group::module super;

 public:

    local_group_module()
    : super("local"), m_process(process_information::get())
    , m_actor_utype(uniform_typeid<actor_ptr>()){ }

    group_ptr get(const std::string& identifier) {
        shared_guard guard(m_instances_mtx);
        auto i = m_instances.find(identifier);
        if (i != m_instances.end()) {
            return i->second;
        }
        else {
            local_group_ptr tmp(new local_group(true, this, identifier));
            { // lifetime scope of uguard
                upgrade_guard uguard(guard);
                auto p = m_instances.insert(std::make_pair(identifier, tmp));
                // someone might preempt us
                return p.first->second;
            }
        }
    }

    intrusive_ptr<group> deserialize(deserializer* source) {
        // deserialize {identifier, process_id, node_id}
        auto pv_identifier = source->read_value(pt_u8string);
        auto& identifier = cppa::get<std::string>(pv_identifier);
        // deserialize broker
        actor_ptr broker;
        m_actor_utype->deserialize(&broker, source);
        CPPA_REQUIRE(broker != nullptr);
        if (!broker) return nullptr;
        if (broker->parent_process() == process()) {
            return this->get(identifier);
        }
        else {
            auto& pinf = broker->parent_process();
            shared_guard guard(m_proxies_mtx);
            auto& node_map = m_proxies[pinf];
            auto i = node_map.find(identifier);
            if (i != node_map.end()) {
                return i->second;
            }
            else {
                local_group_ptr tmp(new local_group_proxy(broker, this,
                                                          identifier,
                                                          broker->parent_process_ptr()));
                upgrade_guard uguard(guard);
                auto p = node_map.insert(std::make_pair(identifier, tmp));
                // someone might preempt us
                return p.first->second;
            }
        }
    }

    void serialize(local_group* ptr, serializer* sink) {
        // serialize identifier & broker
        sink->write_value(ptr->identifier());
        CPPA_REQUIRE(ptr->broker() != nullptr);
        m_actor_utype->serialize(&ptr->broker(), sink);
    }

    inline const process_information& process() const {
        return *m_process;
    }

 private:

    typedef std::map<std::string, local_group_ptr> local_group_map;

    process_information_ptr m_process;
    const uniform_type_info* m_actor_utype;
    util::shared_spinlock m_instances_mtx;
    local_group_map m_instances;
    util::shared_spinlock m_proxies_mtx;
    std::map<process_information, local_group_map> m_proxies;

};

local_group::local_group(bool spawn_local_broker,
                         local_group_module* mod,
                         std::string id,
                         process_information_ptr parent)
: group(mod, std::move(id)), m_process(std::move(parent)) {
    if (spawn_local_broker) {
        m_broker = spawn_hidden<local_broker>(this);
    }
}

void local_group::serialize(serializer* sink) {
    // this cast is safe, because the only available constructor accepts
    // local_group_module* as module pointer
    static_cast<local_group_module*>(m_module)->serialize(this, sink);
}

std::atomic<size_t> m_ad_hoc_id;

} // namespace <anonymous>

namespace cppa { namespace detail {

group_manager::group_manager() {
    group::unique_module_ptr ptr(new local_group_module);
    m_mmap.insert(std::make_pair(std::string("local"), std::move(ptr)));
}

intrusive_ptr<group> group_manager::anonymous() {
    std::string id = "__#";
    id += std::to_string(++m_ad_hoc_id);
    return get_module("local")->get(id);
}

intrusive_ptr<group> group_manager::get(const std::string& module_name,
                                        const std::string& group_identifier) {
    auto mod = get_module(module_name);
    if (mod) {
        return mod->get(group_identifier);
    }
    std::string error_msg = "no module named \"";
    error_msg += module_name;
    error_msg += "\" found";
    throw std::logic_error(error_msg);
}

void group_manager::add_module(std::unique_ptr<group::module> mptr) {
    if (!mptr) return;
    const std::string& mname = mptr->name();
    { // lifetime scope of guard
        std::lock_guard<std::mutex> guard(m_mmap_mtx);
        if (m_mmap.insert(std::make_pair(mname, std::move(mptr))).second) {
            return; // success; don't throw
        }
    }
    std::string error_msg = "module name \"";
    error_msg += mname;
    error_msg += "\" already defined";
    throw std::logic_error(error_msg);
}

group::module* group_manager::get_module(const std::string& module_name) {
    std::lock_guard<std::mutex> guard(m_mmap_mtx);
    auto i = m_mmap.find(module_name);
    return  (i != m_mmap.end()) ? i->second.get() : nullptr;
}

} } // namespace cppa::detail
