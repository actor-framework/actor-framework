/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <iostream>
#include <utility>

#include "caf/logger.hpp"
#include "caf/ref_counted.hpp"
#include "caf/string_algorithms.hpp"

#include "caf/opencl/global.hpp"
#include "caf/opencl/device.hpp"
#include "caf/opencl/opencl_err.hpp"

using namespace std;

namespace caf {
namespace opencl {

device_ptr device::create(const detail::raw_context_ptr& context,
                          const detail::raw_device_ptr& device_id,
                          unsigned id) {
  CAF_LOG_DEBUG("creating device for opencl device with id:" << CAF_ARG(id));
  // look up properties we need to create the command queue
  auto supported = info<cl_ulong>(device_id, CL_DEVICE_QUEUE_PROPERTIES);
  bool profiling = false; // (supported & CL_QUEUE_PROFILING_ENABLE) != 0u;
  bool out_of_order = (supported & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) != 0u;
  unsigned properties = profiling ? CL_QUEUE_PROFILING_ENABLE : 0;
  if (out_of_order)
    properties |= CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
  // create the command queue
  detail::raw_command_queue_ptr command_queue{
    v2get(CAF_CLF(clCreateCommandQueue),
          context.get(), device_id.get(),
          properties),
    false
  };
  // create the device
  auto dev = make_counted<device>(device_id, std::move(command_queue),
                                  context, id);
  //device dev{device_id, std::move(command_queue), context, id};
  // look up device properties
  dev->address_bits_ = info<cl_uint>(device_id, CL_DEVICE_ADDRESS_BITS);
  dev->little_endian_ = info<cl_bool>(device_id, CL_DEVICE_ENDIAN_LITTLE);
  dev->global_mem_cache_size_ =
    info<cl_ulong>(device_id, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE);
  dev->global_mem_cacheline_size_ =
    info<cl_uint>(device_id, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE);
  dev->global_mem_size_ =
    info<cl_ulong >(device_id,CL_DEVICE_GLOBAL_MEM_SIZE);
  dev->host_unified_memory_ =
    info<cl_bool>(device_id, CL_DEVICE_HOST_UNIFIED_MEMORY);
  dev->local_mem_size_ =
    info<cl_ulong>(device_id, CL_DEVICE_LOCAL_MEM_SIZE);
  dev->local_mem_type_ =
    info<cl_uint>(device_id, CL_DEVICE_LOCAL_MEM_TYPE);
  dev->max_clock_frequency_ =
    info<cl_uint>(device_id, CL_DEVICE_MAX_CLOCK_FREQUENCY);
  dev->max_compute_units_ =
    info<cl_uint>(device_id, CL_DEVICE_MAX_COMPUTE_UNITS);
  dev->max_constant_args_ =
    info<cl_uint>(device_id, CL_DEVICE_MAX_CONSTANT_ARGS);
  dev->max_constant_buffer_size_ =
    info<cl_ulong>(device_id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE);
  dev->max_mem_alloc_size_ =
    info<cl_ulong>(device_id, CL_DEVICE_MAX_MEM_ALLOC_SIZE);
  dev->max_parameter_size_ =
    info<size_t>(device_id, CL_DEVICE_MAX_PARAMETER_SIZE);
  dev->max_work_group_size_ =
    info<size_t>(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE);
  dev->max_work_item_dimensions_ =
    info<cl_uint>(device_id, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS);
  dev->profiling_timer_resolution_ =
    info<size_t>(device_id, CL_DEVICE_PROFILING_TIMER_RESOLUTION);
  dev->max_work_item_sizes_.resize(dev->max_work_item_dimensions_);
  clGetDeviceInfo(device_id.get(), CL_DEVICE_MAX_WORK_ITEM_SIZES,
                  sizeof(size_t) * dev->max_work_item_dimensions_,
                  dev->max_work_item_sizes_.data(), nullptr);
  dev->device_type_ =
    device_type_from_ulong(info<cl_ulong>(device_id, CL_DEVICE_TYPE));
  string extensions = info_string(device_id, CL_DEVICE_EXTENSIONS);
  split(dev->extensions_, extensions, " ", false);
  dev->opencl_c_version_ = info_string(device_id, CL_DEVICE_EXTENSIONS);
  dev->device_vendor_ = info_string(device_id, CL_DEVICE_VENDOR);
  dev->device_version_ = info_string(device_id, CL_DEVICE_VERSION);
  dev->name_ = info_string(device_id, CL_DEVICE_NAME);
  return dev;
}

void device::synchronize() {
  clFinish(queue_.get());
}

string device::info_string(const detail::raw_device_ptr& device_id,
                           unsigned info_flag) {
  size_t size;
  clGetDeviceInfo(device_id.get(), info_flag, 0, nullptr, &size);
  vector<char> buffer(size);
  clGetDeviceInfo(device_id.get(), info_flag, sizeof(char) * size, buffer.data(),
                  nullptr);
  return string(buffer.data());
}

device::device(detail::raw_device_ptr device_id,
               detail::raw_command_queue_ptr queue,
               detail::raw_context_ptr context,
               unsigned id)
  : device_id_(std::move(device_id)),
    queue_(std::move(queue)),
    context_(std::move(context)),
    id_(id) {
  // nop
}

device::~device() {
  // nop
}

} // namespace opencl
} // namespace caf
