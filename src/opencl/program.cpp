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

namespace cppa { namespace opencl {


program::program(context_ptr context, command_queue_ptr queue, program_ptr program)
: m_context(move(context)), m_program(move(program)), m_queue(move(queue)) { }

program program::create(const char* kernel_source, uint32_t device_id) {
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

    auto program_build_info = [&](size_t vs, void* v, size_t* vsr) {
        return clGetProgramBuildInfo(pptr.get(),
                                     devices[device_id].m_device.get(),
                                     CL_PROGRAM_BUILD_LOG,
                                     vs,
                                     v,
                                     vsr);
    };

    // build programm from program object
    auto dev_tmp = devices[device_id].m_device.get();
    err = clBuildProgram(pptr.get(), 1, &dev_tmp, nullptr, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        ostringstream oss;
        oss << "clBuildProgram: " << get_opencl_error(err);
        size_t ret_size;
        auto bi_err = program_build_info(0, nullptr, &ret_size);
        if (bi_err == CL_SUCCESS) {
            vector<char> build_log(ret_size);
            bi_err = program_build_info(ret_size, build_log.data(), nullptr);
            if (bi_err == CL_SUCCESS) {
                if (ret_size <= 1) {
                    oss << " (no build log available)";
                }
                else {
                    build_log[ret_size - 1] = '\0';
                    cerr << build_log.data() << endl;
                }
            }
            else { // program_build_info failed to get log
                oss << " (with CL_PROGRAM_BUILD_LOG to get log)";
            }
        }
        else { // program_build_info failed to get size of the log
            oss << " (with CL_PROGRAM_BUILD_LOG to get size)";
        }
        CPPA_LOGM_ERROR(detail::demangle<program>().c_str(), oss.str());
        throw runtime_error(oss.str());
    }
#   ifdef CPPA_DEBUG_MODE
    else {
        const char* where = "CL_PROGRAM_BUILD_LOG:get size";
        size_t ret_size;
        err = program_build_info(0, nullptr, &ret_size);
        if (err == CL_SUCCESS) {
            where = "CL_PROGRAM_BUILD_LOG:get log";
            vector<char> build_log(ret_size+1);
            err = program_build_info(ret_size, build_log.data(), nullptr);
            if (err == CL_SUCCESS) {
                if (ret_size > 1) {
                    CPPA_LOGM_ERROR(detail::demangle<program>().c_str(),
                                    "clBuildProgram: error");
                    build_log[ret_size - 1] = '\0';
                    cerr << "clBuildProgram build log:\n"
                         << build_log.data() << endl;
                }
            }
        }
        if (err != CL_SUCCESS) {
            CPPA_LOGM_ERROR(detail::demangle<program>().c_str(),
                            "clGetProgramBuildInfo (" << where << "): "
                            << get_opencl_error(err));
        }
    }
#   endif
    return {context, devices[device_id].m_cmd_queue, pptr};
}

} } // namespace cppa::opencl
