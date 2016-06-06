/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 * Raphael Hiesgen <raphael.hiesgen (at) haw-hamburg.de>                      *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/type_list.hpp"

#include "caf/opencl/device.hpp"
#include "caf/opencl/manager.hpp"
#include "caf/opencl/platform.hpp"
#include "caf/opencl/opencl_err.hpp"

using namespace std;

namespace caf {
namespace opencl {

const optional<const device&> manager::get_device(size_t dev_id) const {
  if (platforms_.empty())
    return none;
  size_t to = 0;
  for (auto& pl : platforms_) {
    auto from = to;
    to += pl.get_devices().size();
    if (dev_id >= from && dev_id < to)
      return pl.get_devices()[dev_id - from];
  }
  return none;
}

void manager::init(actor_system_config&) {
  // get number of available platforms
  auto num_platforms = v1get<cl_uint>(CAF_CLF(clGetPlatformIDs));
  // get platform ids
  std::vector<cl_platform_id> platform_ids(num_platforms);
  v2callcl(CAF_CLF(clGetPlatformIDs), num_platforms, platform_ids.data());
  if (platform_ids.empty())
    throw std::runtime_error("no OpenCL platform found");
  // initialize platforms (device discovery)
  unsigned current_device_id = 0;
  for (auto& pl_id : platform_ids) {
    platforms_.push_back(platform::create(pl_id, current_device_id));
    current_device_id +=
      static_cast<unsigned>(platforms_.back().get_devices().size());
  }
}

void manager::start() {
  // nop
}

void manager::stop() {
  // nop
}

actor_system::module::id_t manager::id() const {
  return actor_system::module::opencl_manager;
}

void* manager::subtype_ptr() {
  return this;
}

actor_system::module* manager::make(actor_system& sys,
                                    caf::detail::type_list<>) {
  return new manager{sys};
}

program manager::create_program(const char* kernel_source, const char* options,
                                 uint32_t device_id) {
  auto dev = get_device(device_id);
  if (! dev) {
    ostringstream oss;
    oss << "No device with id '" << device_id << "' found.";
    CAF_LOG_ERROR(CAF_ARG(oss.str()));
    throw runtime_error(oss.str());
  }
  return create_program(kernel_source, options, *dev);
}

program manager::create_program(const char* kernel_source, const char* options,
                                 const device& dev) {
  // create program object from kernel source
  size_t kernel_source_length = strlen(kernel_source);
  program_ptr pptr;
  auto rawptr = v2get(CAF_CLF(clCreateProgramWithSource), dev.context_.get(),
                      cl_uint{1}, &kernel_source, &kernel_source_length);
  pptr.reset(rawptr, false);
  // build programm from program object
  auto dev_tmp = dev.device_id_.get();
  cl_int err = clBuildProgram(pptr.get(), 1, &dev_tmp,
                              options, nullptr, nullptr);
  if (err != CL_SUCCESS) {
    ostringstream oss;
    oss << "clBuildProgram: " << get_opencl_error(err);
// the build log will be printed by the pfn_notify (see opencl/manger.cpp)
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
      ostringstream ss;
      ss << "Build log:\n" << string(buffer.data())
         << "\n#######################################";
      CAF_LOG_ERROR(CAF_ARG(ss.str()));
    }
#endif
    throw runtime_error(oss.str());
  }
  map<string, kernel_ptr> available_kernels;
  cl_uint number_of_kernels = 0;
  clCreateKernelsInProgram(pptr.get(), 0, nullptr, &number_of_kernels);
  vector<cl_kernel> kernels(number_of_kernels);
  err = clCreateKernelsInProgram(pptr.get(), number_of_kernels, kernels.data(),
                                 nullptr);
  if (err != CL_SUCCESS) {
    ostringstream oss;
    oss << "clCreateKernelsInProgram: " << get_opencl_error(err);
    throw runtime_error(oss.str());
  } else {
    for (cl_uint i = 0; i < number_of_kernels; ++i) {
      size_t ret_size;
      clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, 0, nullptr, &ret_size);
      vector<char> name(ret_size);
      err = clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, ret_size,
                            reinterpret_cast<void*>(name.data()), nullptr);
      if (err != CL_SUCCESS) {
        ostringstream oss;
        oss << "clGetKernelInfo (CL_KERNEL_FUNCTION_NAME): "
            << get_opencl_error(err);
        throw runtime_error(oss.str());
      }
      kernel_ptr kernel;
      kernel.reset(move(kernels[i]));
      available_kernels.emplace(string(name.data()), move(kernel));
    }
  }
  return {dev.context_, dev.command_queue_, pptr, move(available_kernels)};
}

manager::manager(actor_system& sys) : system_(sys){
  // nop
}

manager::~manager() {
  // nop
}

} // namespace opencl
} // namespace caf
