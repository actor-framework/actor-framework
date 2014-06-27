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


#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/message.hpp"
#include "cppa/singletons.hpp"

#include "cppa/detail/group_manager.hpp"

namespace cppa {

group::group(const invalid_group_t&) : m_ptr(nullptr) { }

group::group(abstract_group_ptr ptr) : m_ptr(std::move(ptr)) { }

group& group::operator=(const invalid_group_t&) {
    m_ptr.reset();
    return *this;
}

intptr_t group::compare(const group& other) const {
    return channel::compare(m_ptr.get(), other.m_ptr.get());
}

group group::get(const std::string& arg0, const std::string& arg1) {
    return get_group_manager()->get(arg0, arg1);
}

group group::anonymous() {
    return get_group_manager()->anonymous();
}

void group::add_module(abstract_group::unique_module_ptr ptr) {
    get_group_manager()->add_module(std::move(ptr));
}

abstract_group::module_ptr group::get_module(const std::string& module_name) {
    return get_group_manager()->get_module(module_name);
}

} // namespace cppa
