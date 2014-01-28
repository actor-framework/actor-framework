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


#include <set>
#include <mutex>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <condition_variable>

#include "cppa/cppa.hpp"
#include "cppa/group.hpp"
#include "cppa/to_string.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/message_header.hpp"
#include "cppa/event_based_actor.hpp"

#include "cppa/io/middleman.hpp"

#include "cppa/detail/raw_access.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/group_manager.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

namespace {

using namespace std;
using namespace cppa;

typedef lock_guard<util::shared_spinlock> exclusive_guard;
typedef util::shared_lock_guard<util::shared_spinlock> shared_guard;
typedef util::upgrade_lock_guard<util::shared_spinlock> upgrade_guard;

class local_broker;
class local_group_module;

class local_group : public abstract_group {

 public:

    void send_all_subscribers(const message_header& hdr, const any_tuple& msg) {
        CPPA_LOG_TRACE(CPPA_TARG(hdr.sender, to_string) << ", "
                       << CPPA_TARG(msg, to_string));
        shared_guard guard(m_mtx);
        for (auto& s : m_subscribers) {
            s.enqueue(hdr, msg);
        }
    }

    void enqueue(const message_header& hdr, any_tuple msg) override {
        CPPA_LOG_TRACE(CPPA_TARG(hdr, to_string) << ", "
                       << CPPA_TARG(msg, to_string));
        send_all_subscribers(hdr, msg);
        m_broker->enqueue(hdr, msg);
    }

    pair<bool, size_t> add_subscriber(const channel& who) {
        CPPA_LOG_TRACE(CPPA_TARG(who, to_string));
        exclusive_guard guard(m_mtx);
        if (m_subscribers.insert(who).second) {
            return {true, m_subscribers.size()};
        }
        return {false, m_subscribers.size()};
    }

    pair<bool, size_t> erase_subscriber(const channel& who) {
        CPPA_LOG_TRACE(CPPA_TARG(who, to_string));
        exclusive_guard guard(m_mtx);
        auto success = m_subscribers.erase(who) > 0;
        return {success, m_subscribers.size()};
    }

    abstract_group::subscription subscribe(const channel& who) {
        CPPA_LOG_TRACE(CPPA_TARG(who, to_string));
        if (add_subscriber(who).first) {
            return {who, this};
        }
        return {};
    }

    void unsubscribe(const channel& who) {
        CPPA_LOG_TRACE(CPPA_TARG(who, to_string));
        erase_subscriber(who);
    }

    void serialize(serializer* sink);

    const actor& broker() const {
        return m_broker;
    }

    local_group(bool spawn_local_broker, local_group_module* mod, string id);

 protected:

    util::shared_spinlock m_mtx;
    set<channel> m_subscribers;
    actor m_broker;

};

typedef intrusive_ptr<local_group> local_group_ptr;

class local_broker : public event_based_actor {

 public:

    local_broker(local_group_ptr g) : m_group(move(g)) { }

    behavior make_behavior() override {
        return (
            on(atom("JOIN"), arg_match) >> [=](const actor& other) {
                CPPA_LOGC_TRACE("cppa::local_broker", "init$JOIN",
                                CPPA_TARG(other, to_string));
                if (other && m_acquaintances.insert(other).second) {
                    monitor(other);
                }
            },
            on(atom("LEAVE"), arg_match) >> [=](const actor& other) {
                CPPA_LOGC_TRACE("cppa::local_broker", "init$LEAVE",
                                CPPA_TARG(other, to_string));
                if (other && m_acquaintances.erase(other) > 0) {
                    demonitor(other);
                }
            },
            on(atom("FORWARD"), arg_match) >> [=](const any_tuple& what) {
                CPPA_LOGC_TRACE("cppa::local_broker", "init$FORWARD",
                                CPPA_TARG(what, to_string));
                // local forwarding
                message_header hdr{last_sender(), nullptr};
                m_group->send_all_subscribers(hdr, what);
                // forward to all acquaintances
                send_to_acquaintances(what);
            },
            on_arg_match >> [=](const down_msg&) {
                auto sender = last_sender();
                CPPA_LOGC_TRACE("cppa::local_broker", "init$DOWN",
                                CPPA_TARG(sender, to_string));
                if (sender) {
                    auto first = m_acquaintances.begin();
                    auto last = m_acquaintances.end();
                    auto i = std::find_if(first, last, [=](const actor& a) {
                        return a == sender;
                    });
                    if (i != last) m_acquaintances.erase(i);
                }
            },
            others() >> [=] {
                auto msg = last_dequeued();
                CPPA_LOGC_TRACE("cppa::local_broker", "init$others",
                                CPPA_TARG(msg, to_string));
                send_to_acquaintances(msg);
            }
        );
    }

 private:

    void send_to_acquaintances(const any_tuple& what) {
        // send to all remote subscribers
        auto sender = last_sender();
        CPPA_LOG_DEBUG("forward message to " << m_acquaintances.size()
                       << " acquaintances; " << CPPA_TSARG(sender)
                       << ", " << CPPA_TSARG(what));
        for (auto& acquaintance : m_acquaintances) {
            acquaintance->enqueue({sender, acquaintance}, what);
        }
    }

    local_group_ptr m_group;
    set<actor> m_acquaintances;

};

// Send a "JOIN" message to the original group if a proxy
// has local subscriptions and a "LEAVE" message to the original group
// if there's no subscription left.

class proxy_broker;

class local_group_proxy : public local_group {

    typedef local_group super;

 public:

    template<typename... Ts>
    local_group_proxy(actor remote_broker, Ts&&... args)
    : super(false, forward<Ts>(args)...) {
        CPPA_REQUIRE(m_broker == invalid_actor);
        CPPA_REQUIRE(remote_broker != invalid_actor);
        m_broker = move(remote_broker);
        m_proxy_broker = spawn<proxy_broker, hidden>(this);
    }

    abstract_group::subscription subscribe(const channel& who) {
        CPPA_LOG_TRACE(CPPA_TSARG(who));
        auto res = add_subscriber(who);
        if (res.first) {
            if (res.second == 1) {
                // join the remote source
                anon_send(m_broker, atom("JOIN"), m_proxy_broker);
            }
            return {who, this};
        }
        CPPA_LOG_WARNING("channel " << to_string(who) << " already joined");
        return {};
    }

    void unsubscribe(const channel& who) {
        CPPA_LOG_TRACE(CPPA_TSARG(who));
        auto res = erase_subscriber(who);
        if (res.first && res.second == 0) {
            // leave the remote source,
            // because there's no more subscriber on this node
            anon_send(m_broker, atom("LEAVE"), m_proxy_broker);
        }
    }

    void enqueue(const message_header& hdr, any_tuple msg) override {
        // forward message to the broker
        m_broker->enqueue(hdr, make_any_tuple(atom("FORWARD"), move(msg)));
    }

 private:

    actor m_proxy_broker;

};

typedef intrusive_ptr<local_group_proxy> local_group_proxy_ptr;

class proxy_broker : public event_based_actor {

 public:

    proxy_broker(local_group_proxy_ptr grp) : m_group(move(grp)) { }

    behavior make_behavior() {
        return (
            others() >> [=] {
                message_header hdr{last_sender(), nullptr};
                m_group->send_all_subscribers(hdr, last_dequeued());
            }
        );
    }

 private:

    local_group_proxy_ptr m_group;

};

class local_group_module : public abstract_group::module {

    typedef abstract_group::module super;

 public:

    local_group_module()
    : super("local"), m_process(node_id::get())
    , m_actor_utype(uniform_typeid<actor>()){ }

    group get(const string& identifier) override {
        shared_guard guard(m_instances_mtx);
        auto i = m_instances.find(identifier);
        if (i != m_instances.end()) {
            return {i->second};
        }
        else {
            auto tmp = make_counted<local_group>(true, this, identifier);
            { // lifetime scope of uguard
                upgrade_guard uguard(guard);
                auto p = m_instances.insert(make_pair(identifier, tmp));
                // someone might preempt us
                return {p.first->second};
            }
        }
    }

    group deserialize(deserializer* source) override {
        // deserialize {identifier, process_id, node_id}
        auto identifier = source->read<string>();
        // deserialize broker
        actor broker;
        m_actor_utype->deserialize(&broker, source);
        if (!broker) return invalid_group;
        if (!broker->is_remote()) {
            return this->get(identifier);
        }
        else {
            shared_guard guard(m_proxies_mtx);
            auto i = m_proxies.find(broker);
            if (i != m_proxies.end()) {
                return {i->second};
            }
            else {
                local_group_ptr tmp{new local_group_proxy{broker, this,
                                                          identifier}};
                upgrade_guard uguard(guard);
                auto p = m_proxies.insert(make_pair(broker, tmp));
                // someone might preempt us
                return {p.first->second};
            }
        }
    }

    void serialize(local_group* ptr, serializer* sink) {
        // serialize identifier & broker
        sink->write_value(ptr->identifier());
        CPPA_REQUIRE(ptr->broker() != invalid_actor);
        m_actor_utype->serialize(&ptr->broker(), sink);
    }

    inline const node_id& process() const {
        return *m_process;
    }

 private:

    node_id_ptr m_process;
    const uniform_type_info* m_actor_utype;
    util::shared_spinlock m_instances_mtx;
    map<string, local_group_ptr> m_instances;
    util::shared_spinlock m_proxies_mtx;
    map<actor, local_group_ptr> m_proxies;

};

class remote_group : public abstract_group {

    typedef abstract_group super;

 public:

    remote_group(abstract_group::module_ptr parent, string id, local_group_ptr decorated)
    : super(parent, move(id)), m_decorated(decorated) { }

    abstract_group::subscription subscribe(const channel& who) {
        CPPA_LOG_TRACE(CPPA_TSARG(who));
        return m_decorated->subscribe(who);
    }

    void unsubscribe(const channel&) {
        CPPA_LOG_ERROR("should never be called");
    }

    void enqueue(const message_header& hdr, any_tuple msg) override {
        CPPA_LOG_TRACE("");
        m_decorated->enqueue(hdr, std::move(msg));
    }

    void serialize(serializer* sink);

    void group_down() {
        CPPA_LOG_TRACE("");
        group this_group{this};
        m_decorated->send_all_subscribers({invalid_actor_addr, nullptr},
                                          make_any_tuple(atom("GROUP_DOWN"),
                                                         this_group));
    }

 private:

    local_group_ptr m_decorated;

};

typedef intrusive_ptr<remote_group> remote_group_ptr;

class shared_map : public ref_counted {

    typedef unique_lock<mutex> lock_type;

 public:

    remote_group_ptr get(const string& key) {
        remote_group_ptr result;
        { // lifetime scope of guard
            lock_type guard(m_mtx);
            auto i = m_instances.find(key);
            if (i == m_instances.end()) {
                anon_send(m_worker, atom("FETCH"), key);
                do {
                    m_cond.wait(guard);
                } while ((i = m_instances.find(key)) == m_instances.end());

            }
            result = i->second;
        }
        if (!result) {
            throw network_error("could not connect to remote group");
        }
        return result;
    }

    group peek(const string& key) {
        lock_type guard(m_mtx);
        auto i = m_instances.find(key);
        if (i != m_instances.end()) {
            return {i->second};
        }
        return invalid_group;
    }

    void put(const string& key, const remote_group_ptr& ptr) {
        lock_type guard(m_mtx);
        m_instances[key] = ptr;
        m_cond.notify_all();
    }

    actor m_worker;

 private:

    mutex m_mtx;
    condition_variable m_cond;
    map<string, remote_group_ptr> m_instances;

};

typedef intrusive_ptr<shared_map> shared_map_ptr;

class remote_group_module : public abstract_group::module {

    typedef abstract_group::module super;

 public:

    remote_group_module() : super("remote") {
        auto sm = make_counted<shared_map>();
        abstract_group::module_ptr this_group{this};
        m_map = sm;
        m_map->m_worker = spawn<hidden>([=](event_based_actor* self) -> behavior {
            CPPA_LOGC_TRACE(detail::demangle(typeid(*this_group)),
                            "remote_group_module$worker",
                            "");
            typedef map<string, pair<actor, vector<pair<string, remote_group_ptr>>>>
                    peer_map;
            auto peers = std::make_shared<peer_map>();
            return (
                on(atom("FETCH"), arg_match) >> [=](const string& key) {
                    // format is group@host:port
                    auto pos1 = key.find('@');
                    auto pos2 = key.find(':');
                    auto last = string::npos;
                    string authority;
                    if (pos1 != last && pos2 != last && pos1 < pos2) {
                        auto name = key.substr(0, pos1);
                        authority = key.substr(pos1 + 1);
                        auto i = peers->find(authority);
                        actor nameserver;
                        if (i != peers->end()) {
                            nameserver = i->second.first;
                        }
                        else {
                            auto host = key.substr(pos1 + 1, pos2 - pos1 - 1);
                            auto pstr = key.substr(pos2 + 1);
                            istringstream iss(pstr);
                            uint16_t port;
                            if (iss >> port) {
                                try {
                                    nameserver = remote_actor(host, port);
                                    self->monitor(nameserver);
                                    (*peers)[authority].first = nameserver;
                                }
                                catch (exception&) {
                                    sm->put(key, nullptr);
                                    return;
                                }
                            }
                        }
                        self->timed_sync_send(nameserver, chrono::seconds(10), atom("GET_GROUP"), name).then (
                            on(atom("GROUP"), arg_match) >> [&](const group& g) {
                                auto gg = dynamic_cast<local_group*>(detail::raw_access::get(g));
                                if (gg) {
                                    auto rg = make_counted<remote_group>(this_group, key, gg);
                                    sm->put(key, rg);
                                    (*peers)[authority].second.push_back(make_pair(key, rg));
                                }
                                else {
                                    cerr << "*** WARNING: received a non-local "
                                            "group form nameserver for key "
                                         << key << " in file "
                                         << __FILE__
                                         << ", line " << __LINE__
                                         << endl;
                                    sm->put(key, nullptr);
                                }
                            },
                            on<sync_timeout_msg>() >> [sm, &key] {
                                sm->put(key, nullptr);
                            }
                        );
                    }
                },
                on_arg_match >> [&](const down_msg&) {
                    auto who = self->last_sender();
                    auto find_peer = [&] {
                        return find_if(begin(*peers), end(*peers), [&](const peer_map::value_type& kvp) {
                            return kvp.second.first == who;
                        });
                    };
                    for (auto i = find_peer(); i != peers->end(); i = find_peer()) {
                        for (auto& kvp: i->second.second) {
                            sm->put(kvp.first, nullptr);
                            kvp.second->group_down();
                        }
                        peers->erase(i);
                    }
                },
                others() >> [] { }
            );
        });
    }

    group get(const std::string& group_name) {
        return {m_map->get(group_name)};
    }

    group deserialize(deserializer* source) {
        return get(source->read<string>());
    }

    void serialize(abstract_group* ptr, serializer* sink) {
        sink->write_value(ptr->identifier());
    }

 private:

    shared_map_ptr m_map;

};

local_group::local_group(bool spawn_local_broker,
                         local_group_module* mod,
                         string id)
: abstract_group(mod, move(id)) {
    if (spawn_local_broker) m_broker = spawn<local_broker, hidden>(this);
}

void local_group::serialize(serializer* sink) {
    // this cast is safe, because the only available constructor accepts
    // local_group_module* as module pointer
    static_cast<local_group_module*>(m_module)->serialize(this, sink);
}

void remote_group::serialize(serializer* sink) {
    static_cast<remote_group_module*>(m_module)->serialize(this, sink);
}

atomic<size_t> m_ad_hoc_id;

} // namespace <anonymous>

namespace cppa { namespace detail {

group_manager::group_manager() {
    abstract_group::unique_module_ptr ptr(new local_group_module);
    m_mmap.insert(make_pair(string("local"), move(ptr)));
    ptr.reset(new remote_group_module);
    m_mmap.insert(make_pair(string("remote"), move(ptr)));
}

group group_manager::anonymous() {
    string id = "__#";
    id += std::to_string(++m_ad_hoc_id);
    return get_module("local")->get(id);
}

group group_manager::get(const string& module_name,
                         const string& group_identifier) {
    auto mod = get_module(module_name);
    if (mod) {
        return mod->get(group_identifier);
    }
    string error_msg = "no module named \"";
    error_msg += module_name;
    error_msg += "\" found";
    throw logic_error(error_msg);
}

void group_manager::add_module(unique_ptr<abstract_group::module> mptr) {
    if (!mptr) return;
    const string& mname = mptr->name();
    { // lifetime scope of guard
        lock_guard<mutex> guard(m_mmap_mtx);
        if (m_mmap.insert(make_pair(mname, move(mptr))).second) {
            return; // success; don't throw
        }
    }
    string error_msg = "module name \"";
    error_msg += mname;
    error_msg += "\" already defined";
    throw logic_error(error_msg);
}

abstract_group::module* group_manager::get_module(const string& module_name) {
    lock_guard<mutex> guard(m_mmap_mtx);
    auto i = m_mmap.find(module_name);
    return  (i != m_mmap.end()) ? i->second.get() : nullptr;
}

} } // namespace cppa::detail
