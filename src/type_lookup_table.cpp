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


#include <algorithm>

#include "cppa/singletons.hpp"
#include "cppa/type_lookup_table.hpp"

#include "cppa/detail/uniform_type_info_map.hpp"

namespace cppa {

type_lookup_table::type_lookup_table() {
    auto uti_map = get_uniform_type_info_map();
    auto get = [=](const char* cstr) {
        return uti_map->by_uniform_name(cstr);
    };
    emplace(1, get("@<>+@atom"));
    emplace(2, get("@<>+@atom+@u32"));
    emplace(3, get("@<>+@atom+@proc"));
    emplace(4, get("@<>+@atom+@proc+@u32"));
    emplace(5, get("@<>+@atom+@proc+@u32+@u32"));
    emplace(6, get("@<>+@atom+@actor"));
    emplace(7, get("@<>+@atom+@u32+@str"));
}

auto type_lookup_table::by_id(std::uint32_t id) const -> pointer {
    auto i = find(id);
    return (i == m_data.end() || i->first != id) ? nullptr : i->second;
}

auto type_lookup_table::by_name(const std::string& name) const -> pointer {
    auto e = m_data.end();
    auto i = std::find_if(m_data.begin(), e, [&](const value_type& val) {
        return val.second->name() == name;
    });
    return (i != e) ? i->second : nullptr;
}

std::uint32_t type_lookup_table::id_of(const std::string& name) const {
    auto e = m_data.end();
    auto i = std::find_if(m_data.begin(), e, [&](const value_type& val) {
        return val.second->name() == name;
    });
    return (i != e) ? i->first : 0;
}

std::uint32_t type_lookup_table::id_of(pointer uti) const {
    auto e = m_data.end();
    auto i = std::find_if(m_data.begin(), e, [&](const value_type& val) {
        return val.second == uti;
    });
    return (i != e) ? i->first : 0;
}


void type_lookup_table::emplace(std::uint32_t id, pointer instance) {
    CPPA_REQUIRE(instance != nullptr);
    value_type kvp{id, instance};
    auto i = find(id);
    if (i == m_data.end()) m_data.push_back(std::move(kvp));
    else if (i->first == id) throw std::runtime_error("key already defined");
    else m_data.insert(i, std::move(kvp));
}

auto type_lookup_table::find(std::uint32_t arg) const -> const_iterator {
    return std::lower_bound(m_data.begin(), m_data.end(), arg, [](const value_type& lhs, std::uint32_t id) {
        return lhs.first < id;
    });
}

auto type_lookup_table::find(std::uint32_t arg) -> iterator {
    return std::lower_bound(m_data.begin(), m_data.end(), arg, [](const value_type& lhs, std::uint32_t id) {
        return lhs.first < id;
    });
}

std::uint32_t type_lookup_table::max_id() const {
    return m_data.empty() ? 0 : m_data.back().first;
}


} // namespace cppa
