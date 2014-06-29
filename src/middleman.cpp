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


#include <tuple>
#include <cerrno>
#include <memory>
#include <cstring>
#include <sstream>
#include <iostream>
#include <stdexcept>

#include "cppa/on.hpp"
#include "cppa/actor.hpp"
#include "cppa/config.hpp"
#include "cppa/node_id.hpp"
#include "cppa/to_string.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/io/middleman.hpp"
#include "cppa/io/remote_actor_proxy.hpp"

#include "cppa/detail/logging.hpp"
#include "cppa/detail/ripemd_160.hpp"
#include "cppa/detail/safe_equal.hpp"
#include "cppa/detail/singletons.hpp"
#include "cppa/detail/make_counted.hpp"
#include "cppa/detail/get_root_uuid.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/get_mac_addresses.hpp"

#ifdef CPPA_WINDOWS
#   include <io.h>
#   include <fcntl.h>
#endif

namespace cppa {
namespace io {

using detail::make_counted;

middleman* middleman::instance() {
    auto mpi = detail::singletons::middleman_plugin_id;
    return static_cast<middleman*>(detail::singletons::get_plugin_singleton(mpi, [] {
        return new middleman;
    }));
}

void middleman::add_broker(broker_ptr bptr) {
    m_brokers.emplace(bptr);
    bptr->attach_functor([=](uint32_t) {
        m_brokers.erase(bptr);
    });
}

void middleman::initialize() {
    CPPA_LOG_TRACE("");
    m_supervisor = new network::supervisor{m_backend};
    m_thread = std::thread([this] {
        CPPA_LOGC_TRACE("cppa::io::middleman", "initialize$run", "");
        m_backend.run();
    });
    m_backend.m_tid = m_thread.get_id();
}

void middleman::stop() {
    CPPA_LOG_TRACE("");
    m_backend.dispatch([=] {
        CPPA_LOGC_TRACE("cppa::io::middleman", "stop$lambda", "");
        delete m_supervisor;
        m_supervisor = nullptr;
        // m_managers will be modified while we are stopping each manager,
        // because each manager will call remove(...)
        std::vector<broker_ptr> brokers;
        for (auto& kvp : m_named_brokers) brokers.push_back(kvp.second);
        for (auto& bro : brokers) bro->close_all();
    });
    m_thread.join();
    m_named_brokers.clear();
}

void middleman::dispose() {
    delete this;
}

middleman::middleman() : m_supervisor(nullptr) { }

middleman::~middleman() { }

} // namespace io
} // namespace cppa
