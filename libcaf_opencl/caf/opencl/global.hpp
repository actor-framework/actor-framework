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

#ifndef CAF_OPENCL_GLOBAL_HPP
#define CAF_OPENCL_GLOBAL_HPP

#include <string>

#include "caf/config.hpp"
#include "caf/detail/limited_vector.hpp"

#ifdef CAF_MACOS
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

/**
 * A vector of up to three elements used for OpenCL dimensions.
 */
using dim_vec = detail::limited_vector<size_t, 3>;

std::string get_opencl_error(cl_int err);

} // namespace opencl
} // namespace caf

#endif // CAF_OPENCL_GLOBAL_HPP
