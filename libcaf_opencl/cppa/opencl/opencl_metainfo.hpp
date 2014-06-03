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
 * Copyright (C) 2011-2014                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 * Raphael Hiesgen <raphael.hiesgen@haw-hamburg.de>                           *
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


#ifndef CPPA_OPENCL_METAINFO_HPP
#define CPPA_OPENCL_METAINFO_HPP

#include <atomic>
#include <vector>
#include <algorithm>
#include <functional>

#include "cppa/cppa.hpp"

#include "cppa/opencl/global.hpp"
#include "cppa/opencl/program.hpp"
#include "cppa/opencl/smart_ptr.hpp"
#include "cppa/opencl/device_info.hpp"
#include "cppa/opencl/actor_facade.hpp"

#include "cppa/detail/singleton_mixin.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {
namespace opencl {

class opencl_metainfo {

    friend class program;
    friend class detail::singleton_manager;
    friend command_queue_ptr get_command_queue(uint32_t id);

 public:

    const std::vector<device_info> get_devices() const;

 private:

    static inline opencl_metainfo* create_singleton() {
        return new opencl_metainfo;
    }

    void initialize();
    void dispose();
    void destroy();

    context_ptr m_context;
    std::vector<device_info> m_devices;

};

opencl_metainfo* get_opencl_metainfo();

} // namespace opencl
} // namespace cppa

#endif // CPPA_OPENCL_METAINFO_HPP
