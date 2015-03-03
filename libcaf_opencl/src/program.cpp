/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <vector>
#include <string>
#include <cstring>
#include <iostream>

#include "caf/detail/singletons.hpp"

#include "caf/opencl/program.hpp"
#include "caf/opencl/opencl_err.hpp"
#include "caf/opencl/opencl_metainfo.hpp"

using namespace std;

namespace caf {
namespace opencl {

program::program(context_ptr context, command_queue_ptr queue,
                 program_ptr program)
    : m_context(move(context)),
      m_program(move(program)),
      m_queue(move(queue)) {}

program program::create(const char* kernel_source, const char* options,
                        uint32_t device_id) {
  auto metainfo = opencl_metainfo::instance();
  auto devices  = metainfo->get_devices();
  auto context  = metainfo->m_context;
  if (devices.size() <= device_id) {
    ostringstream oss;
    oss << "Device id " << device_id
        << " is not a vaild device. Maximum id is: " << (devices.size() - 1)
        << ".";
    CAF_LOGF_ERROR(oss.str());
    throw runtime_error(oss.str());
  }
  // create program object from kernel source
  size_t kernel_source_length = strlen(kernel_source);
  program_ptr pptr;
  auto rawptr = v2get(CAF_CLF(clCreateProgramWithSource),context.get(),
                      cl_uint{1}, &kernel_source, &kernel_source_length);
  pptr.reset(rawptr, false);
  // build programm from program object
  auto dev_tmp = devices[device_id].m_device.get();
  cl_int err = clBuildProgram(pptr.get(), 1, &dev_tmp,
                              options,nullptr, nullptr);
  if (err != CL_SUCCESS) {
    ostringstream oss;
    oss << "clBuildProgram: " << get_opencl_error(err);
// the build log will be printed by the
// pfn_notify (see opencl_metainfo.cpp)
#ifndef __APPLE__
    // seems that just apple implemented the
    // pfn_notify callback, but we can get
    // the build log
    if (err == CL_BUILD_PROGRAM_FAILURE) {
      size_t buildlog_buffer_size = 0;
      // get the log length
      clGetProgramBuildInfo(pptr.get(), dev_tmp, CL_PROGRAM_BUILD_LOG,
                            sizeof(buildlog_buffer_size), nullptr,
                            &buildlog_buffer_size);

      vector<char> buffer(buildlog_buffer_size);

      // fill the buffer with buildlog informations
      clGetProgramBuildInfo(pptr.get(), dev_tmp, CL_PROGRAM_BUILD_LOG,
                            sizeof(buffer[0]) * buildlog_buffer_size,
                            buffer.data(), nullptr);

      CAF_LOGC_ERROR("caf::opencl::program", "create",
                     "Build log:\n" + string(buffer.data()) +
                       "\n########################################");
    }
#endif
    throw runtime_error(oss.str());
  }
  return {context, devices[device_id].m_cmd_queue, pptr};
}

} // namespace opencl
} // namespace caf
