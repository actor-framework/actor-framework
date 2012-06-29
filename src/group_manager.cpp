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
#include "cppa/detail/group_manager.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

namespace {

using namespace cppa;

typedef std::lock_guard<util::shared_spinlock> exclusive_guard;
typedef util::shared_lock_guard<util::shared_spinlock> shared_guard;
typedef util::upgrade_lock_guard<util::shared_spinlock> upgrade_guard;

class local_group_module;

class group_impl : public group {

    util::shared_spinlock m_shared_mtx;
    std::set<channel_ptr> m_subscribers;

 protected:

    template<typename F, typename S>
    group_impl(F&& f, S&& s) : group(std::forward<F>(f), std::forward<S>(s)) { }

 public:

    void enqueue(actor* sender, any_tuple msg) /*override*/ {
        shared_guard guard(m_shared_mtx);
        for (auto i = m_subscribers.begin(); i != m_subscribers.end(); ++i) {
            // this cast is safe because we don't affect the "value"
            // of *i, thus, the set remains in a consistent state
            const_cast<channel_ptr&>(*i)->enqueue(sender, msg);
        }
    }

    group::subscription subscribe(const channel_ptr& who) /*override*/ {
        group::subscription result;
        exclusive_guard guard(m_shared_mtx);
        if (m_subscribers.insert(who).second) {
            result.reset(new group::unsubscriber(who, this));
        }
        return result;
    }

    void unsubscribe(const channel_ptr& who) /*override*/ {
        exclusive_guard guard(m_shared_mtx);
        m_subscribers.erase(who);
    }
};

struct anonymous_group : group_impl {
    anonymous_group() : group_impl("anonymous", "anonymous") { }
};

class local_group : public group_impl {

    friend class local_group_module;

    local_group(const std::string& gname) : group_impl(gname, "local") { }

};

class local_group_module : public group::module {

    typedef group::module super;

    util::shared_spinlock m_mtx;
    std::map<std::string, group_ptr> m_instances;

 public:

    local_group_module() : super("local") { }

    group_ptr get(const std::string& group_name) {
        shared_guard guard(m_mtx);
        auto i = m_instances.find(group_name);
        if (i != m_instances.end()) {
            return i->second;
        }
        else {
            group_ptr tmp(new local_group(group_name));
            { // lifetime scope of uguard
                upgrade_guard uguard(guard);
                auto p = m_instances.insert(std::make_pair(group_name, tmp));
                // someone might preempt us
                return p.first->second;
            }
        }
    }

};

} // namespace <anonymous>

namespace cppa { namespace detail {

group_manager::group_manager() {
    std::unique_ptr<group::module> ptr(new local_group_module);
    m_mmap.insert(std::make_pair(std::string("local"), std::move(ptr)));
}

intrusive_ptr<group> group_manager::anonymous() {
    return new anonymous_group;
}

intrusive_ptr<group> group_manager::get(const std::string& module_name,
                                        const std::string& group_identifier) {
    { // lifetime scope of guard
        std::lock_guard<std::mutex> guard(m_mmap_mtx);
        auto i = m_mmap.find(module_name);
        if (i != m_mmap.end()) {
            return (i->second)->get(group_identifier);
        }
    }
    std::string error_msg = "no module named \"";
    error_msg += module_name;
    error_msg += "\" found";
    throw std::logic_error(error_msg);
}

void group_manager::add_module(group::module* mod) {
    const std::string& mname = mod->name();
    std::unique_ptr<group::module> mptr(mod);
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

} } // namespace cppa::detail
