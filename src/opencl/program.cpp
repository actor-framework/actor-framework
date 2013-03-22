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

#include <vector>
#include <cstring>

#include "cppa/opencl/program.hpp"
#include "cppa/opencl/command_dispatcher.hpp"

namespace cppa { namespace opencl {

//program::program() : m_context(nullptr), pptr(nullptr) { }

program::program(context_ptr context, program_ptr program)
: m_context(std::move(context)), m_program(std::move(program)) { }

program program::create(const char* kernel_source) {
    context_ptr cptr = get_command_dispatcher()->m_context;

    cl_int err{0};

    // create program object from kernel source
    program_ptr pptr;
    size_t kernel_source_length = strlen(kernel_source);
    pptr.adopt(clCreateProgramWithSource(cptr.get(),
                                         1,
                                         &kernel_source,
                                         &kernel_source_length,
                                         &err));

    if (err != CL_SUCCESS) {
        throw std::runtime_error("clCreateProgramWithSource: '"
                                 + get_opencl_error(err)
                                 + "'.");
    }

    // build programm from program object
    err = clBuildProgram(pptr.get(), 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        device_ptr device_used(cppa::detail::singleton_manager::
                               get_command_dispatcher()->
                               m_devices.front().dev_id);
        cl_build_status build_status;
        err = clGetProgramBuildInfo(pptr.get(),
                                    device_used.get(),
                                    CL_PROGRAM_BUILD_STATUS,
                                    sizeof(cl_build_status),
                                    &build_status,
                                    NULL);
        size_t ret_val_size;
        err = clGetProgramBuildInfo(pptr.get(),
                                    device_used.get(),
                                    CL_PROGRAM_BUILD_LOG,
                                    0,
                                    NULL,
                                    &ret_val_size);
        std::vector<char> build_log(ret_val_size+1);
        err = clGetProgramBuildInfo(pptr.get(),
                                    device_used.get(),
                                    CL_PROGRAM_BUILD_LOG,
                                    ret_val_size,
                                    build_log.data(),
                                    NULL);
        build_log[ret_val_size] = '\0';
        throw std::runtime_error("[!!!] clBuildProgram: '"
                            + get_opencl_error(err)
                            + "'. Build log: "
                            + build_log.data());
    }
    else {
#ifdef CPPA_DEBUG
        device_ptr device_used(cppa::detail::singleton_manager::
                               get_command_dispatcher()->
                               m_devices.front().dev_id);
        cl_build_status build_status;
        err = clGetProgramBuildInfo(pptr.get(),
                                    device_used.get(),
                                    CL_PROGRAM_BUILD_STATUS,
                                    sizeof(cl_build_status),
                                    &build_status,
                                    NULL);
        size_t ret_val_size;
        err = clGetProgramBuildInfo(pptr.get(),
                                    device_used.get(),
                                    CL_PROGRAM_BUILD_LOG,
                                    0,
                                    NULL,
                                    &ret_val_size);
        std::vector<char> build_log(ret_val_size+1);
        err = clGetProgramBuildInfo(pptr.get(),
                                    device_used.get(),
                                    CL_PROGRAM_BUILD_LOG,
                                    ret_val_size,
                                    build_log.data(),
                                    NULL);
        build_log[ret_val_size] = '\0';
        std::cout << "clBuildProgram log: '"
                  << build_log.data()
                  << std::endl;
#endif
    }
    return {cptr, pptr};
}

} } // namespace cppa::opencl
