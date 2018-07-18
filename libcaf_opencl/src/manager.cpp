/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include <fstream>

#include "caf/detail/type_list.hpp"
#include "caf/raise_error.hpp"

#include "caf/opencl/device.hpp"
#include "caf/opencl/manager.hpp"
#include "caf/opencl/platform.hpp"
#include "caf/opencl/opencl_err.hpp"

using namespace std;

namespace caf {
namespace opencl {

optional<device_ptr> manager::find_device(size_t dev_id) const {
  if (platforms_.empty())
    return none;
  size_t to = 0;
  for (auto& pl : platforms_) {
    auto from = to;
    to += pl->devices().size();
    if (dev_id >= from && dev_id < to)
      return pl->devices()[dev_id - from];
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
    CAF_RAISE_ERROR("no OpenCL platform found");
  // initialize platforms (device discovery)
  unsigned current_device_id = 0;
  for (auto& pl_id : platform_ids) {
    platforms_.push_back(platform::create(pl_id, current_device_id));
    current_device_id +=
      static_cast<unsigned>(platforms_.back()->devices().size());
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

program_ptr manager::create_program_from_file(const char* path,
                                              const char* options,
                                              uint32_t device_id) {
  std::ifstream read_source{std::string(path), std::ios::in};
  string kernel_source;
  if (read_source) {
    read_source.seekg(0, std::ios::end);
    kernel_source.resize(static_cast<size_t>(read_source.tellg()));
    read_source.seekg(0, std::ios::beg);
    read_source.read(&kernel_source[0],
                     static_cast<streamsize>(kernel_source.size()));
    read_source.close();
  } else {
    CAF_RAISE_ERROR("create_program_from_file: path not found");
  }
  return create_program(kernel_source.c_str(), options, device_id);
}

program_ptr manager::create_program(const char* kernel_source,
                                    const char* options,
                                    uint32_t device_id) {
  auto dev = find_device(device_id);
  if (!dev) {
    CAF_RAISE_ERROR("create_program: no device found");
  }
  return create_program(kernel_source, options, *dev);
}

program_ptr manager::create_program_from_file(const char* path,
                                              const char* options,
                                              const device_ptr dev) {
  std::ifstream read_source{std::string(path), std::ios::in};
  string kernel_source;
  if (read_source) {
    read_source.seekg(0, std::ios::end);
    kernel_source.resize(static_cast<size_t>(read_source.tellg()));
    read_source.seekg(0, std::ios::beg);
    read_source.read(&kernel_source[0],
                     static_cast<streamsize>(kernel_source.size()));
    read_source.close();
  } else {
    CAF_RAISE_ERROR("create_program_from_file: path not found");
  }
  return create_program(kernel_source.c_str(), options, dev);
}

program_ptr manager::create_program(const char* kernel_source,
                                    const char* options,
                                    const device_ptr dev) {
  // create program object from kernel source
  size_t kernel_source_length = strlen(kernel_source);
  detail::raw_program_ptr pptr;
  pptr.reset(v2get(CAF_CLF(clCreateProgramWithSource), dev->context_.get(),
                           1u, &kernel_source, &kernel_source_length),
             false);
  // build programm from program object
  auto dev_tmp = dev->device_id_.get();
  auto err = clBuildProgram(pptr.get(), 1, &dev_tmp, options, nullptr, nullptr);
  if (err != CL_SUCCESS) {
    if (err == CL_BUILD_PROGRAM_FAILURE) {
      size_t buildlog_buffer_size = 0;
      // get the log length
      clGetProgramBuildInfo(pptr.get(), dev_tmp, CL_PROGRAM_BUILD_LOG,
                            0, nullptr, &buildlog_buffer_size);
      vector<char> buffer(buildlog_buffer_size);
      // fill the buffer with buildlog informations
      clGetProgramBuildInfo(pptr.get(), dev_tmp, CL_PROGRAM_BUILD_LOG,
                            sizeof(char) * buildlog_buffer_size,
                            buffer.data(), nullptr);
      ostringstream ss;
      ss << "############## Build log ##############"
         << endl << string(buffer.data()) << endl
         << "#######################################";
      // seems that just apple implemented the
      // pfn_notify callback, but we can get
      // the build log
#ifndef CAF_MACOS
      CAF_LOG_ERROR(CAF_ARG(ss.str()));
#endif
    }
    CAF_RAISE_ERROR("clBuildProgram failed");
  }
  cl_uint number_of_kernels = 0;
  clCreateKernelsInProgram(pptr.get(), 0u, nullptr, &number_of_kernels);
  map<string, detail::raw_kernel_ptr> available_kernels;
  if (number_of_kernels > 0) {
    vector<cl_kernel> kernels(number_of_kernels);
    err = clCreateKernelsInProgram(pptr.get(), number_of_kernels,
                                   kernels.data(), nullptr);
    if (err != CL_SUCCESS)
      CAF_RAISE_ERROR("clCreateKernelsInProgram failed");
    for (cl_uint i = 0; i < number_of_kernels; ++i) {
      size_t len;
      clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, 0, nullptr, &len);
      vector<char> name(len);
      err = clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, len,
                            reinterpret_cast<void*>(name.data()), nullptr);
      if (err != CL_SUCCESS)
        CAF_RAISE_ERROR("clGetKernelInfo failed");
      detail::raw_kernel_ptr kernel;
      kernel.reset(move(kernels[i]));
      available_kernels.emplace(string(name.data()), move(kernel));
    }
  } else {
    CAF_LOG_WARNING("Could not built all kernels in program. Since this happens"
                    " on some platforms, we'll ignore this and try to build"
                    " each kernel individually by name.");
  }
  return make_counted<program>(dev->context_, dev->queue_, pptr,
                               move(available_kernels));
}

manager::manager(actor_system& sys) : system_(sys) {
  // nop
}

manager::~manager() {
  // nop
}

} // namespace opencl
} // namespace caf
