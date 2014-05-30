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


#ifndef CPPA_TYPE_LOOKUP_TABLE_HPP
#define CPPA_TYPE_LOOKUP_TABLE_HPP

#include <vector>
#include <memory>
#include <utility>

#include "cppa/uniform_type_info.hpp"

namespace cppa {

/**
 *
 * Default types are:
 *
 *  1: {atom_value}
 *  2: {atom_value, uint32_t}
 *  3: {atom_value, node_id}
 *  4: {atom_value, node_id, uint32_t}
 *  5: {atom_value, node_id, uint32_t, uint32_t}
 *  6: {atom_value, actor_ptr}
 *  7: {atom_value, uint32_t, string}
 *
 */
class type_lookup_table {

 public:

    typedef const uniform_type_info* pointer;

    type_lookup_table();

    pointer by_id(std::uint32_t id) const;

    pointer by_name(const std::string& name) const;

    std::uint32_t id_of(const std::string& name) const;

    std::uint32_t id_of(pointer uti) const;

    void emplace(std::uint32_t id, pointer instance);

    std::uint32_t max_id() const;

 private:

    typedef std::vector<std::pair<std::uint32_t, pointer>> container;
    typedef container::value_type value_type;
    typedef container::iterator iterator;
    typedef container::const_iterator const_iterator;

    container m_data;

    const_iterator find(std::uint32_t) const;

    iterator find(std::uint32_t);

};

} // namespace cppa

#endif // CPPA_TYPE_LOOKUP_TABLE_HPP
