/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CPPA_OPENCL_GLOBAL_HPP
#define CPPA_OPENCL_GLOBAL_HPP

#include <string>

#include "cppa/util/limited_vector.hpp"

#if defined __APPLE__ || defined(MACOSX)
    #include <OpenCL/opencl.h>
#else
    #include <CL/opencl.h>
#endif

namespace cppa {
namespace opencl {

/**
 * @brief A vector of up to three elements used for OpenCL dimensions.
 */
typedef util::limited_vector<size_t, 3> dim_vec;

std::string get_opencl_error(cl_int err);

cl_int clReleaseDeviceDummy (cl_device_id);
cl_int clRetainDeviceDummy  (cl_device_id);

} // namespace opencl
} // namespace cppa

#endif // CPPA_OPENCL_GLOBAL_HPP
