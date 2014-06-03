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


#ifndef CPPA_OPENCL_DEVICE_INFO_HPP
#define CPPA_OPENCL_DEVICE_INFO_HPP

#include "cppa/opencl/global.hpp"
#include "cppa/opencl/program.hpp"
#include "cppa/opencl/smart_ptr.hpp"

namespace cppa {
namespace opencl {

class device_info {

    friend class program;

 public:

    device_info(device_ptr device,
                command_queue_ptr queue,
                size_t work_group_size,
                cl_uint dimensons,
                const dim_vec& items_per_dimension)
        : m_max_work_group_size(work_group_size)
        , m_max_dimensions(dimensons)
        , m_max_work_items_per_dim(items_per_dimension)
        , m_device(device)
        , m_cmd_queue(queue) { }

    inline size_t get_max_work_group_size();
    inline cl_uint get_max_dimensions();
    inline dim_vec get_max_work_items_per_dim();

 private:

    size_t  m_max_work_group_size;
    cl_uint m_max_dimensions;
    dim_vec m_max_work_items_per_dim;
    device_ptr m_device;
    command_queue_ptr m_cmd_queue;
};

/******************************************************************************\
 *                 implementation of inline member functions                  *
\******************************************************************************/

inline size_t device_info::get_max_work_group_size() {
    return m_max_work_group_size;
}

inline cl_uint device_info::get_max_dimensions() {
    return m_max_dimensions;
}

inline dim_vec device_info::get_max_work_items_per_dim() {
    return m_max_work_items_per_dim;
}

} // namespace opencl
} // namespace cppa


#endif // CPPA_OPENCL_DEVICE_INFO_HPP
