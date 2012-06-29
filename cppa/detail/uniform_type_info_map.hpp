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


#ifndef CPPA_UNIFORM_TYPE_INFO_MAP_HPP
#define CPPA_UNIFORM_TYPE_INFO_MAP_HPP

#include <set>
#include <string>
#include <utility> // std::pair

#include "cppa/detail/default_uniform_type_info_impl.hpp"

namespace cppa { class uniform_type_info; }

namespace cppa { namespace detail {

class uniform_type_info_map_helper;

// note: this class is implemented in uniform_type_info.cpp
class uniform_type_info_map {

    friend class uniform_type_info_map_helper;

 public:

    typedef std::set<std::string> set_type;
    typedef std::map<std::string, uniform_type_info*> uti_map_type;
    typedef std::map<int, std::pair<set_type, set_type> > int_map_type;

    uniform_type_info_map();

    ~uniform_type_info_map();

    inline const int_map_type& int_names() const {
        return m_ints;
    }

    const uniform_type_info* by_raw_name(const std::string& name) const;

    const uniform_type_info* by_uniform_name(const std::string& name) const;

    std::vector<const uniform_type_info*> get_all() const;

    // NOT thread safe!
    bool insert(const std::set<std::string>& raw_names, uniform_type_info* uti);

 private:

    // maps raw typeid names to uniform type informations
    uti_map_type m_by_rname;

    // maps uniform names to uniform type informations
    uti_map_type m_by_uname;

    // maps sizeof(-integer_type-) to { signed-names-set, unsigned-names-set }
    int_map_type m_ints;

};

} } // namespace cppa::detail

#endif // CPPA_UNIFORM_TYPE_INFO_MAP_HPP
