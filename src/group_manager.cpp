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

#include "cppa/any_tuple.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"

#include "cppa/detail/middleman.hpp"
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

class local_group_module;

class local_group : public group {

 public:

    void enqueue(actor* sender, any_tuple msg) {
        shared_guard guard(m_shared_mtx);
        for (auto& s : m_subscribers) {
            s->enqueue(sender, msg);
        }
    }

    group::subscription subscribe(const channel_ptr& who) {
        exclusive_guard guard(m_shared_mtx);
        if (m_subscribers.insert(who).second) {
            return {who, this};
        }
        return {};
    }

    void unsubscribe(const channel_ptr& who) {
        exclusive_guard guard(m_shared_mtx);
        m_subscribers.erase(who);
    }

    void serialize(serializer* sink);

    inline const process_information& process() const {
        return *m_process;
    }

    inline const process_information_ptr& process_ptr() const {
        return m_process;
    }

    local_group(local_group_module* mod, std::string id,
                process_information_ptr parent = process_information::get());

 private:

    util::shared_spinlock m_shared_mtx;
    std::set<channel_ptr> m_subscribers;
    process_information_ptr m_process;

};

class local_group_proxy : public local_group {

    typedef local_group super;

 public:

    template<typename... Args>
    local_group_proxy(Args&&... args) : super(std::forward<Args>(args)...) { }

    void enqueue(actor* sender, any_tuple msg) {
        detail::middleman_enqueue(process_ptr(), sender, this, std::move(msg));
    }

    void remote_enqueue(actor* sender, any_tuple msg) {
        super::enqueue(sender, std::move(msg));
    }

};

typedef intrusive_ptr<local_group> local_group_ptr;

class local_group_module : public group::module {

    typedef group::module super;

 public:

    local_group_module()
    : super("local"), m_process(process_information::get()) { }

    group_ptr get(const std::string& identifier) {
        shared_guard guard(m_instances_mtx);
        auto i = m_instances.find(identifier);
        if (i != m_instances.end()) {
            return i->second;
        }
        else {
            local_group_ptr tmp(new local_group(this, identifier));
            { // lifetime scope of uguard
                upgrade_guard uguard(guard);
                auto p = m_instances.insert(std::make_pair(identifier, tmp));
                // someone might preempt us
                return p.first->second;
            }
        }
    }

    intrusive_ptr<group> deserialize(deserializer* source) {
        primitive_variant ptup[3];
        primitive_type ptypes[] = {pt_u8string, pt_uint32, pt_u8string};
        source->read_tuple(3, ptypes, ptup);
        auto& identifier = cppa::get<std::string>(ptup[0]);
        auto  process_id = cppa::get<std::uint32_t>(ptup[1]);
        auto& node_id    = cppa::get<std::string>(ptup[2]);
        if (   process_id == process().process_id()
            && equal(node_id, process().node_id())) {
            return this->get(identifier);
        }
        else {
            process_information pinf(process_id, node_id);
            shared_guard guard(m_proxies_mtx);
            auto& node_map = m_proxies[pinf];
            auto i = node_map.find(identifier);
            if (i != node_map.end()) {
                return i->second;
            }
            else {
                local_group_ptr tmp(new local_group_proxy(this, identifier));
                process_information_ptr piptr;
                // re-use process_information_ptr from another proxy if possible
                if (node_map.empty()) {
                    piptr.reset(new process_information(pinf));
                } else {
                    piptr = node_map.begin()->second->process_ptr();
                }
                upgrade_guard uguard(guard);
                auto p = node_map.insert(std::make_pair(identifier, tmp));
                // someone might preempt us
                return p.first->second;
            }
        }
    }

    void serialize(local_group* ptr, serializer* sink) {
        primitive_variant ptup[3];
        ptup[0] = ptr->identifier();
        ptup[1] = ptr->process().process_id();
        ptup[2] = to_string(ptr->process().node_id());
        sink->write_tuple(3, ptup);
    }

    inline const process_information& process() const {
        return *m_process;
    }

 private:

    typedef std::map<std::string, local_group_ptr> local_group_map;

    process_information_ptr m_process;
    util::shared_spinlock m_instances_mtx;
    local_group_map m_instances;
    util::shared_spinlock m_proxies_mtx;
    std::map<process_information, local_group_map> m_proxies;

};

local_group::local_group(local_group_module* mod,
                         std::string id,
                         process_information_ptr parent)
: group(mod, std::move(id)), m_process(std::move(parent)) { }

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
