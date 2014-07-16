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

#include "caf/on.hpp"
#include "caf/actor.hpp"
#include "caf/config.hpp"
#include "caf/node_id.hpp"
#include "caf/to_string.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/remote_actor_proxy.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/ripemd_160.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/make_counted.hpp"
#include "caf/detail/get_root_uuid.hpp"
#include "caf/detail/actor_registry.hpp"
#include "caf/detail/get_mac_addresses.hpp"

#ifdef CAF_WINDOWS
#   include <io.h>
#   include <fcntl.h>
#endif

namespace caf {
namespace io {

using detail::make_counted;

middleman* middleman::instance() {
    auto mpi = detail::singletons::middleman_plugin_id;
    return static_cast<middleman*>(detail::singletons::get_plugin_singleton(mpi, [] {
        return new middleman;
    }));
}

void middleman::add_broker(broker_ptr bptr) {
    m_brokers.insert(bptr);
    bptr->attach_functor([=](uint32_t) {
        m_brokers.erase(bptr);
    });
}

void middleman::initialize() {
    CAF_LOG_TRACE("");
    m_supervisor = new network::supervisor{m_backend};
    m_thread = std::thread([this] {
        CAF_LOGC_TRACE("caf::io::middleman", "initialize$run", "");
        m_backend.run();
    });
    m_backend.m_tid = m_thread.get_id();
}

void middleman::stop() {
    CAF_LOG_TRACE("");
    m_backend.dispatch([=] {
        CAF_LOGC_TRACE("caf::io::middleman", "stop$lambda", "");
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
} // namespace caf
