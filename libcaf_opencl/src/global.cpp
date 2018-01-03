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

#include "caf/opencl/global.hpp"

cl_int clReleaseDeviceDummy(cl_device_id) {
  return 0;
}

cl_int clRetainDeviceDummy(cl_device_id) {
  return 0;
}

namespace caf {
namespace opencl {

std::ostream& operator<<(std::ostream& os, device_type dev) {
  switch(dev) {
    case def         : os << "default";     break;
    case cpu         : os << "CPU";         break;
    case gpu         : os << "GPU";         break;
    case accelerator : os << "accelerator"; break;
    case custom      : os << "custom";      break;
    case all         : os << "all";         break;
    default : os.setstate(std::ios_base::failbit);
  }
  return os;
}

device_type device_type_from_ulong(cl_ulong dev) {
  switch(dev) {
    case CL_DEVICE_TYPE_CPU         : return cpu;
    case CL_DEVICE_TYPE_GPU         : return gpu;
    case CL_DEVICE_TYPE_ACCELERATOR : return accelerator;
    case CL_DEVICE_TYPE_CUSTOM      : return custom;
    case CL_DEVICE_TYPE_ALL         : return all;
    default : return def;
  }
}

std::string opencl_error(cl_int err) {
  switch (err) {
    case CL_SUCCESS:
      return "CL_SUCCESS";
    case CL_DEVICE_NOT_FOUND:
      return "CL_DEVICE_NOT_FOUND";
    case CL_DEVICE_NOT_AVAILABLE:
      return "CL_DEVICE_NOT_AVAILABLE";
    case CL_COMPILER_NOT_AVAILABLE:
      return "CL_COMPILER_NOT_AVAILABLE";
    case CL_MEM_OBJECT_ALLOCATION_FAILURE:
      return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case CL_OUT_OF_RESOURCES:
      return "CL_OUT_OF_RESOURCES";
    case CL_OUT_OF_HOST_MEMORY:
      return "CL_OUT_OF_HOST_MEMORY";
    case CL_PROFILING_INFO_NOT_AVAILABLE:
      return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case CL_MEM_COPY_OVERLAP:
      return "CL_MEM_COPY_OVERLAP";
    case CL_IMAGE_FORMAT_MISMATCH:
      return "CL_IMAGE_FORMAT_MISMATCH";
    case CL_IMAGE_FORMAT_NOT_SUPPORTED:
      return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case CL_BUILD_PROGRAM_FAILURE:
      return "CL_BUILD_PROGRAM_FAILURE";
    case CL_MAP_FAILURE:
      return "CL_MAP_FAILURE";
    case CL_INVALID_VALUE:
      return "CL_INVALID_VALUE";
    case CL_INVALID_DEVICE_TYPE:
      return "CL_INVALID_DEVICE_TYPE";
    case CL_INVALID_PLATFORM:
      return "CL_INVALID_PLATFORM";
    case CL_INVALID_DEVICE:
      return "CL_INVALID_DEVICE";
    case CL_INVALID_CONTEXT:
      return "CL_INVALID_CONTEXT";
    case CL_INVALID_QUEUE_PROPERTIES:
      return "CL_INVALID_QUEUE_PROPERTIES";
    case CL_INVALID_COMMAND_QUEUE:
      return "CL_INVALID_COMMAND_QUEUE";
    case CL_INVALID_HOST_PTR:
      return "CL_INVALID_HOST_PTR";
    case CL_INVALID_MEM_OBJECT:
      return "CL_INVALID_MEM_OBJECT";
    case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
      return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case CL_INVALID_IMAGE_SIZE:
      return "CL_INVALID_IMAGE_SIZE";
    case CL_INVALID_SAMPLER:
      return "CL_INVALID_SAMPLER";
    case CL_INVALID_BINARY:
      return "CL_INVALID_BINARY";
    case CL_INVALID_BUILD_OPTIONS:
      return "CL_INVALID_BUILD_OPTIONS";
    case CL_INVALID_PROGRAM:
      return "CL_INVALID_PROGRAM";
    case CL_INVALID_PROGRAM_EXECUTABLE:
      return "CL_INVALID_PROGRAM_EXECUTABLE";
    case CL_INVALID_KERNEL_NAME:
      return "CL_INVALID_KERNEL_NAME";
    case CL_INVALID_KERNEL_DEFINITION:
      return "CL_INVALID_KERNEL_DEFINITION";
    case CL_INVALID_KERNEL:
      return "CL_INVALID_KERNEL";
    case CL_INVALID_ARG_INDEX:
      return "CL_INVALID_ARG_INDEX";
    case CL_INVALID_ARG_VALUE:
      return "CL_INVALID_ARG_VALUE";
    case CL_INVALID_ARG_SIZE:
      return "CL_INVALID_ARG_SIZE";
    case CL_INVALID_KERNEL_ARGS:
      return "CL_INVALID_KERNEL_ARGS";
    case CL_INVALID_WORK_DIMENSION:
      return "CL_INVALID_WORK_DIMENSION";
    case CL_INVALID_WORK_GROUP_SIZE:
      return "CL_INVALID_WORK_GROUP_SIZE";
    case CL_INVALID_WORK_ITEM_SIZE:
      return "CL_INVALID_WORK_ITEM_SIZE";
    case CL_INVALID_GLOBAL_OFFSET:
      return "CL_INVALID_GLOBAL_OFFSET";
    case CL_INVALID_EVENT_WAIT_LIST:
      return "CL_INVALID_EVENT_WAIT_LIST";
    case CL_INVALID_EVENT:
      return "CL_INVALID_EVENT";
    case CL_INVALID_OPERATION:
      return "CL_INVALID_OPERATION";
    case CL_INVALID_GL_OBJECT:
      return "CL_INVALID_GL_OBJECT";
    case CL_INVALID_BUFFER_SIZE:
      return "CL_INVALID_BUFFER_SIZE";
    case CL_INVALID_MIP_LEVEL:
      return "CL_INVALID_MIP_LEVEL";
    case CL_INVALID_GLOBAL_WORK_SIZE:
      return "CL_INVALID_GLOBAL_WORK_SIZE";
    // error codes used by extensions
    // see: http://streamcomputing.eu/blog/2013-04-28/opencl-1-2-error-codes/
    case -1000:
      return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
    case -1001:
      return "No valid ICDs found";
    case -1002:
      return "CL_INVALID_D3D10_DEVICE_KHR";
    case -1003:
      return "CL_INVALID_D3D10_RESOURCE_KHR";
    case -1004:
      return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
    case -1005:
      return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
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
    ss << std::string("ERROR ") + std::to_string(s);
    return ss.str();
  }
  cl_command_type t;
  err = clGetEventInfo(e, CL_EVENT_COMMAND_TYPE, sizeof(cl_command_type),
                       &t, nullptr);
  if (err != CL_SUCCESS) {
    ss << std::string("ERROR ") + std::to_string(s);
    return ss.str();
  }
  switch (s) {
    case(CL_QUEUED):
      ss << std::string("CL_QUEUED");
      break;
    case(CL_SUBMITTED):
      ss << std::string("CL_SUBMITTED");
      break;
    case(CL_RUNNING):
      ss << std::string("CL_RUNNING");
      break;
    case(CL_COMPLETE):
      ss << std::string("CL_COMPLETE");
      break;
    default:
      ss << std::string("DEFAULT ") + std::to_string(s);
      return ss.str();
  }
  ss << " / ";
  switch (t) {
    case(CL_COMMAND_NDRANGE_KERNEL):
      ss << std::string("CL_COMMAND_NDRANGE_KERNEL");
      break;
    case(CL_COMMAND_TASK):
      ss << std::string("CL_COMMAND_TASK");
      break;
    case(CL_COMMAND_NATIVE_KERNEL):
      ss << std::string("CL_COMMAND_NATIVE_KERNEL");
      break;
    case(CL_COMMAND_READ_BUFFER):
      ss << std::string("CL_COMMAND_READ_BUFFER");
      break;
    case(CL_COMMAND_WRITE_BUFFER):
      ss << std::string("CL_COMMAND_WRITE_BUFFER");
      break;
    case(CL_COMMAND_COPY_BUFFER):
      ss << std::string("CL_COMMAND_COPY_BUFFER");
      break;
    case(CL_COMMAND_READ_IMAGE):
      ss << std::string("CL_COMMAND_READ_IMAGE");
      break;
    case(CL_COMMAND_WRITE_IMAGE):
      ss << std::string("CL_COMMAND_WRITE_IMAGE");
      break;
    case(CL_COMMAND_COPY_IMAGE):
      ss << std::string("CL_COMMAND_COPY_IMAGE");
      break;
    case(CL_COMMAND_COPY_BUFFER_TO_IMAGE):
      ss << std::string("CL_COMMAND_COPY_BUFFER_TO_IMAGE");
      break;
    case(CL_COMMAND_COPY_IMAGE_TO_BUFFER):
      ss << std::string("CL_COMMAND_COPY_IMAGE_TO_BUFFER");
      break;
    case(CL_COMMAND_MAP_BUFFER):
      ss << std::string("CL_COMMAND_MAP_BUFFER");
      break;
    case(CL_COMMAND_MAP_IMAGE):
      ss << std::string("CL_COMMAND_MAP_IMAGE");
      break;
    case(CL_COMMAND_UNMAP_MEM_OBJECT):
      ss << std::string("CL_COMMAND_UNMAP_MEM_OBJECT");
      break;
    case(CL_COMMAND_MARKER):
      ss << std::string("CL_COMMAND_MARKER");
      break;
    case(CL_COMMAND_ACQUIRE_GL_OBJECTS):
      ss << std::string("CL_COMMAND_ACQUIRE_GL_OBJECTS");
      break;
    case(CL_COMMAND_RELEASE_GL_OBJECTS):
      ss << std::string("CL_COMMAND_RELEASE_GL_OBJECTS");
      break;
    case(CL_COMMAND_READ_BUFFER_RECT):
      ss << std::string("CL_COMMAND_READ_BUFFER_RECT");
      break;
    case(CL_COMMAND_WRITE_BUFFER_RECT):
      ss << std::string("CL_COMMAND_WRITE_BUFFER_RECT");
      break;
    case(CL_COMMAND_COPY_BUFFER_RECT):
      ss << std::string("CL_COMMAND_COPY_BUFFER_RECT");
      break;
    case(CL_COMMAND_USER):
      ss << std::string("CL_COMMAND_USER");
      break;
    case(CL_COMMAND_BARRIER):
      ss << std::string("CL_COMMAND_BARRIER");
      break;
    case(CL_COMMAND_MIGRATE_MEM_OBJECTS):
      ss << std::string("CL_COMMAND_MIGRATE_MEM_OBJECTS");
      break;
    case(CL_COMMAND_FILL_BUFFER):
      ss << std::string("CL_COMMAND_FILL_BUFFER");
      break;
    case(CL_COMMAND_FILL_IMAGE):
      ss << std::string("CL_COMMAND_FILL_IMAGE");
      break;
    default:
      ss << std::string("DEFAULT ") + std::to_string(s);
      return ss.str();
  }
  return ss.str();
}

} // namespace opencl
} // namespace caf
