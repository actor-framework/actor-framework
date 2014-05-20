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


#ifndef CPPA_GROUP_MANAGER_HPP
#define CPPA_GROUP_MANAGER_HPP

#include <map>
#include <mutex>
#include <thread>

#include "cppa/abstract_group.hpp"
#include "cppa/util/shared_spinlock.hpp"

#include "cppa/detail/singleton_mixin.hpp"

namespace cppa {
namespace detail {

class group_manager : public singleton_mixin<group_manager> {

    friend class singleton_mixin<group_manager>;

 public:

    ~group_manager();

    group get(const std::string& module_name,
              const std::string& group_identifier);

    group anonymous();

    void add_module(abstract_group::unique_module_ptr);

    abstract_group::module_ptr get_module(const std::string& module_name);

 private:

    typedef std::map<std::string, abstract_group::unique_module_ptr> modules_map;

    modules_map m_mmap;
    std::mutex m_mmap_mtx;

    group_manager();

};

} // namespace detail
} // namespace cppa

#endif // CPPA_GROUP_MANAGER_HPP
