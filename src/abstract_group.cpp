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

#include "caf/all.hpp"
#include "caf/message.hpp"
#include "caf/abstract_group.hpp"
#include "caf/detail/shared_spinlock.hpp"

#include "caf/detail/group_manager.hpp"
#include "caf/detail/singletons.hpp"

namespace caf {

abstract_group::subscription::subscription(
    const channel& s, const intrusive_ptr<abstract_group>& g)
        : m_subscriber(s), m_group(g) {}

abstract_group::subscription::~subscription() {
    if (valid()) m_group->unsubscribe(m_subscriber);
}

abstract_group::module::module(std::string name) : m_name(std::move(name)) {}

const std::string& abstract_group::module::name() { return m_name; }

abstract_group::abstract_group(abstract_group::module_ptr mod, std::string id)
        : m_module(mod), m_identifier(std::move(id)) {}

const std::string& abstract_group::identifier() const { return m_identifier; }

abstract_group::module_ptr abstract_group::get_module() const {
    return m_module;
}

const std::string& abstract_group::module_name() const {
    return get_module()->name();
}

abstract_group::module::~module() {}

abstract_group::~abstract_group() {}

} // namespace caf
