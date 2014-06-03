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

#include <vector>
#include <string>
#include <cstring>
#include <iostream>

#include "cppa/singletons.hpp"
#include "cppa/opencl/program.hpp"
#include "cppa/opencl/opencl_metainfo.hpp"

using namespace std;

namespace cppa {
namespace opencl {


program::program(context_ptr context, command_queue_ptr queue, program_ptr program)
: m_context(move(context)), m_program(move(program)), m_queue(move(queue)) { }

program program::create(const char* kernel_source, const char* options, uint32_t device_id) {
    auto metainfo = get_opencl_metainfo();
    auto devices  = metainfo->get_devices();
    auto context  = metainfo->m_context;


    if (devices.size() <= device_id) {
        ostringstream oss;
        oss << "Device id " << device_id
            << " is not a vaild device. Maximum id is: "
            << (devices.size() -1) << ".";
        CPPA_LOGM_ERROR(detail::demangle<program>().c_str(), oss.str());
        throw runtime_error(oss.str());
    }

    cl_int err{0};

    // create program object from kernel source
    size_t kernel_source_length = strlen(kernel_source);
    program_ptr pptr;
    pptr.adopt(clCreateProgramWithSource(context.get(),
                                         1,
                                         &kernel_source,
                                         &kernel_source_length,
                                         &err));
    if (err != CL_SUCCESS) {
        throw runtime_error("clCreateProgramWithSource: "
                                 + get_opencl_error(err));
    }

    // build programm from program object
    auto dev_tmp = devices[device_id].m_device.get();
    err = clBuildProgram(pptr.get(), 1, &dev_tmp, options, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        ostringstream oss;
        oss << "clBuildProgram: " << get_opencl_error(err);
        // the build log will be printed by the
        // pfn_notify (see opencl_metainfo.cpp)
#ifndef __APPLE__
        // seems that just apple implemented the
        // pfn_notify callback, but we can get
        // the build log
        if(err == CL_BUILD_PROGRAM_FAILURE) {
            size_t buildlog_buffer_size = 0;
            // get the log length
            clGetProgramBuildInfo(pptr.get(),
                                  dev_tmp,
                                  CL_PROGRAM_BUILD_LOG,
                                  sizeof(buildlog_buffer_size),
                                  nullptr,
                                  &buildlog_buffer_size);

            vector<char> buffer(buildlog_buffer_size);

            // fill the buffer with buildlog informations
            clGetProgramBuildInfo(pptr.get(),
                                  dev_tmp,
                                  CL_PROGRAM_BUILD_LOG,
                                  sizeof(buffer[0]) * buildlog_buffer_size,
                                  buffer.data(),
                                  nullptr);

            CPPA_LOGC_ERROR("cppa::opencl::program",
                            "create",
                            "Build log:\n" + string(buffer.data()) +
                            "\n########################################");
        }
#endif
        throw runtime_error(oss.str());
    }
    return {context, devices[device_id].m_cmd_queue, pptr};
}

} // namespace opencl
} // namespace cppa

