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

#pragma once

#include <string>
#include <iostream>

#include "caf/config.hpp"
#include "caf/detail/limited_vector.hpp"

#if defined(CAF_MACOS) || defined(CAF_IOS)
# include <OpenCL/opencl.h>
#else
# include <CL/opencl.h>
#endif

// needed for OpenCL 1.0 compatibility (works around missing clReleaseDevice)
extern "C" {
cl_int clReleaseDeviceDummy(cl_device_id);
cl_int clRetainDeviceDummy(cl_device_id);
} // extern "C"

namespace caf {
namespace opencl {

enum device_type {
  def         = CL_DEVICE_TYPE_DEFAULT,
  cpu         = CL_DEVICE_TYPE_CPU,
  gpu         = CL_DEVICE_TYPE_GPU,
  accelerator = CL_DEVICE_TYPE_ACCELERATOR,
  custom      = CL_DEVICE_TYPE_CUSTOM,
  all         = CL_DEVICE_TYPE_ALL
};

/// Default values to create OpenCL buffers
enum buffer_type : cl_mem_flags {
  input         = CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY,
  input_output  = CL_MEM_READ_WRITE,
  output        = CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY,
  scratch_space = CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS
};

std::ostream& operator<<(std::ostream& os, device_type dev);
device_type device_type_from_ulong(cl_ulong dev);

/// A vector of up to three elements used for OpenCL dimensions.
using dim_vec = detail::limited_vector<size_t, 3>;

std::string opencl_error(cl_int err);

std::string event_status(cl_event event);


} // namespace opencl
} // namespace caf

