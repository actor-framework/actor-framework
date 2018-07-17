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

#include <utility>
#include <vector>
#include <iostream>

#include "caf/opencl/platform.hpp"
#include "caf/opencl/opencl_err.hpp"

using namespace std;

namespace caf {
namespace opencl {

platform_ptr platform::create(cl_platform_id platform_id,
                              unsigned start_id) {
  vector<unsigned> device_types = {CL_DEVICE_TYPE_GPU,
                                   CL_DEVICE_TYPE_ACCELERATOR,
                                   CL_DEVICE_TYPE_CPU};
  vector<cl_device_id> ids;
  for (cl_device_type device_type : device_types) {
    auto known = ids.size();
    cl_uint discoverd;
    auto err = clGetDeviceIDs(platform_id, device_type, 0, nullptr, &discoverd);
    if (err == CL_DEVICE_NOT_FOUND) {
      continue; // no devices of the type found
    } else if (err != CL_SUCCESS) {
      throwcl("clGetDeviceIDs", err);
    }
    ids.resize(known + discoverd);
    v2callcl(CAF_CLF(clGetDeviceIDs), platform_id, device_type,
             discoverd, (ids.data() + known));
  }
  vector<detail::raw_device_ptr> devices;
  devices.resize(ids.size());
  auto lift = [](cl_device_id ptr) {
    return detail::raw_device_ptr{ptr, false};
  };
  transform(ids.begin(), ids.end(), devices.begin(), lift);
  detail::raw_context_ptr context;
  context.reset(v2get(CAF_CLF(clCreateContext), nullptr,
                      static_cast<unsigned>(ids.size()),
                      ids.data(), pfn_notify, nullptr),
                false);
  vector<device_ptr> device_information;
  for (auto& device_id : devices) {
    device_information.push_back(device::create(context, device_id,
                                                start_id++));
  }
  if (device_information.empty())
    CAF_RAISE_ERROR("no devices for the platform found");
  auto name = platform_info(platform_id, CL_PLATFORM_NAME);
  auto vendor = platform_info(platform_id, CL_PLATFORM_VENDOR);
  auto version = platform_info(platform_id, CL_PLATFORM_VERSION);
  return make_counted<platform>(platform_id, move(context), move(name),
                                move(vendor), move(version),
                                move(device_information));
}

string platform::platform_info(cl_platform_id platform_id,
                               unsigned info_flag) {
  size_t size;
  auto err = clGetPlatformInfo(platform_id, info_flag, 0, nullptr,
                               &size);
  throwcl("clGetPlatformInfo", err);
  vector<char> buffer(size);
  v2callcl(CAF_CLF(clGetPlatformInfo), platform_id, info_flag,
           sizeof(char) * size, buffer.data());
  return string(buffer.data());
}

platform::platform(cl_platform_id platform_id, detail::raw_context_ptr context,
                   string name, string vendor, string version,
                   vector<device_ptr> devices)
  : platform_id_(platform_id),
    context_(std::move(context)),
    name_(move(name)),
    vendor_(move(vendor)),
    version_(move(version)),
    devices_(move(devices)) {
  // nop
}

platform::~platform() {
  // nop
}

} // namespace opencl
} // namespace caf
