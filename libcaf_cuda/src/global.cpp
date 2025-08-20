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

#include <string>
#include <sstream>

#include "caf/cuda/global.hpp"
/*
cl_int clReleaseDeviceDummy(cl_device_id) {
  std::fprintf(stderr,
               "ERROR: clReleaseDeviceDummy() not functional (OpenCL removed).\n");
  std::abort();
}

cl_int clRetainDeviceDummy(cl_device_id) {
  std::fprintf(stderr,
               "ERROR: clRetainDeviceDummy() not functional (OpenCL removed).\n");
  std::abort();
}

namespace caf {
namespace cuda {

std::ostream& operator<<(std::ostream& os, device_type dev) {
  switch(dev) {
    case device_type::def         : os << "default";     break;
    case device_type::cpu         : os << "CPU";         break;
    case device_type::gpu         : os << "GPU";         break;
    case device_type::accelerator : os << "accelerator"; break;
    case device_type::custom      : os << "custom";      break;
    case device_type::all         : os << "all";         break;
    default : os.setstate(std::ios_base::failbit);
  }
  return os;
}

device_type device_type_from_ulong(cl_ulong dev) {
  switch(dev) {
    case CL_DEVICE_TYPE_CPU         : return device_type::cpu;
    case CL_DEVICE_TYPE_GPU         : return device_type::gpu;
    case CL_DEVICE_TYPE_ACCELERATOR : return device_type::accelerator;
    case CL_DEVICE_TYPE_CUSTOM      : return device_type::custom;
    case CL_DEVICE_TYPE_ALL         : return device_type::all;
    default : return device_type::def;
  }
}

std::string opencl_error(cl_int err) {
  switch (err) {
    case CL_SUCCESS: return "CL_SUCCESS";
    // You can fill in other error codes as needed.
    default:
      return "UNKNOWN_ERROR: " + std::to_string(err);
  }
}

std::string event_status(cl_event e) {
  std::stringstream ss;
  cl_int s;
  auto err = clGetEventInfo(e, CL_EVENT_COMMAND_EXECUTION_STATUS,
                            sizeof(cl_int), &s, nullptr);
  if (err != CL_SUCCESS) {
    ss << "ERROR " << s;
    return ss.str();
  }
  cl_command_type t;
  err = clGetEventInfo(e, CL_EVENT_COMMAND_TYPE, sizeof(cl_command_type), &t, nullptr);
  if (err != CL_SUCCESS) {
    ss << "ERROR " << s;
    return ss.str();
  }
  switch (s) {
    case CL_QUEUED:      ss << "CL_QUEUED"; break;
    case CL_SUBMITTED:   ss << "CL_SUBMITTED"; break;
    case CL_RUNNING:     ss << "CL_RUNNING"; break;
    case CL_COMPLETE:    ss << "CL_COMPLETE"; break;
    default:             ss << "DEFAULT " << s; return ss.str();
  }
  ss << " / ";
  switch (t) {
    case CL_COMMAND_NDRANGE_KERNEL: ss << "CL_COMMAND_NDRANGE_KERNEL"; break;
    // add more if needed
    default: ss << "DEFAULT " << t; return ss.str();
  }
  return ss.str();
}

} // namespace opencl
} // namespace caf
*/
